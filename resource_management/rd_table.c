#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <stdbool.h>
#include "rd_table.h"

#define MSG_BUF_SIZE 1024*sizeof(char)
#define SHMKEY 613614
#define SEM_NAME "anothersem"

int shmid;
char * paddr;
sem_t * sem;

resource_descriptor * table; // global table

//deadlock avoidance algorithm
bool safe_state(int pid, int * num_deadlocked){
    //for safety alogrithm
    bool processed[NUM_PROCESSES]; //already processed processes
    int working[NUM_RESOURCES];

    //for max claims
    int need[NUM_PROCESSES][NUM_RESOURCES];
    int avail[NUM_RESOURCES];
    int req[NUM_RESOURCES];
    int alloc[NUM_PROCESSES][NUM_RESOURCES];

    //instanciate the arrays
    for (int i = 0; i < NUM_PROCESSES; i++){
        for (int j = 0; j < NUM_RESOURCES; j++){
            avail[j] = getavailable(j);
            req[j] = getrequest(j, pid);
            alloc[i][j] = getallocation(j,i);
            need[i][j] = getmax(j, i) - getallocation(j, i);
        }
    }

    //set processes as not processed yet and safe sequences as non-exsistant
    for (int i = 0; i < NUM_PROCESSES; i++){
        processed[i] = 0;
    }

    //set working array
    for (int j = 0; j < NUM_RESOURCES; j++){
        working[j] = avail[j];
        
    }

    //Maxiumum claims
    for (int j = 0; j < NUM_RESOURCES; j++){
        if (req[j] > need[pid][j]){ //pid is constant. looping through the resources.
            fprintf(stderr, "P:%d Asked for more than initial need of R:%d\n", pid, j);
            return false;
       }
        
        if (req[j] <= avail[j]){ //maximum claims
            avail[j] -= req[j];
            alloc[pid][j] += req[j];
            need[pid][j] -= req[j];
        } else {
            return false; //requesting more than is available
        }
    }

    //safety algorithm
    int i = 0;
    while ( i < NUM_PROCESSES){
        bool found  = false;
        for(int k = 0; k < NUM_PROCESSES; k++) {
            if (processed[k] == 0) {
                int j = 0;
                for (j = 0; j < NUM_RESOURCES; j++) {
                    //if the need is more than what the process could theoretically get at somepoint, its an unsafe state
                    if (need[k][j] > working[j] && gettype(j)!= SHAREABLE) {
                        break;
                    }
                }
                if (j == NUM_RESOURCES) {
                    for( int p = 0; p < NUM_RESOURCES; p++){
                        working[p] += alloc[k][p];
                    }
                    i++;
                    processed[k] = 1; //this process was processed
                    found = true;
                }
            }
        }
        //if we did not find a safe sequence
        if (found == false) {
            *num_deadlocked = NUM_PROCESSES - i;
            return false; //system in an unsafe state
        } 

    }
    *num_deadlocked = 0;
    return true; //system in a safe state
}

bool deadlock_avoidance(int pid, int * num_deadlocked ){
    bool result = true;
    sem_wait(sem);
    result = safe_state(pid, num_deadlocked);
    sem_post(sem);
    return !result;
}

// modify a specific element of a resource_descriptor using enum for rd elements
int modify_rd(int rd_num, int pid, int elem, int newval){

    if(rd_num > NUM_RESOURCES ||  rd_num < 0){
       fprintf(stderr, "modify resource_descriptor: id does not exsist.\n");
    } else {

        sem_wait(sem);
        
        switch(elem)
        {
            case REQ:
                setrequest(rd_num, pid, newval);
                break;
            case ALLOC:
                setallocation(rd_num, pid, newval);
                break;
            case AVAIL:
                setavailable(rd_num, newval);
                break;
            case MAX:
                setmax(rd_num, pid, newval);
                break;
            case ID:
                setID(newval);
                break;
            default:
                //fprintf(stderr, "cannot modify resource with rd_num %d\n", rd_num);
                break;
        }
        sem_post(sem);
    }
        return 0;
}

// get a specific element of the rd using enum for rd elements
int getelement(int rd_num, int pid, int elem){
    int result;
    if(rd_num > NUM_RESOURCES || rd_num < 0){
        fprintf(stderr, "get resource_descriptor: id %d does not exsist.\n", rd_num);
    } else {

        sem_wait(sem);

        switch(elem)
        {
            case REQ:
                result = getrequest(rd_num, pid);
                break;
            case ALLOC:
                result =  getallocation(rd_num, pid);
                break;
            case AVAIL:
                result = getavailable(rd_num);
                break;
            case MAX:
                result = getmax(rd_num, pid);
                break;
            case TYPE:
                result = gettype(rd_num);
                break;
            default:
                fprintf(stderr, "cannot get element %d for process with sim_rd_num %d\n", elem, rd_num);
                result =  -1;
                break;
        }
        sem_post(sem);
    }
        return result;
}
//getters and setters for rd itself. all need to be protected by semaphores!
int getrequest(int rd, int pid){
    return table->request[pid][rd];
}
int getallocation(int rd, int pid){
    return table->allocation[pid][rd];
}
int getavailable(int rd){
    return table->available[rd];
    
}
int getmax(int rd, int pid){
    return table->maximum[pid][rd];
}
int gettype(int rd) {
    return table->type[rd];
}
int setrequest(int rd, int pid, int request_num){
    table->request[pid][rd] += request_num;
    return 0;
}
int setallocation(int rd, int pid,  int allocation_num){
    table->allocation[pid][rd] += allocation_num;
    return 0;
}
int setavailable(int rd, int total_num){
    table->available[rd] += total_num;
    return 0;
}
int setmax(int rd, int pid, int newval){
    table->maximum[pid][rd] = newval;
    return 0;
}
int setID(int id) {
    table->ID = id;
    return 0;
}

// initializes a single rd block
int init_rd(int rd_num, int num_resources, int rand_seed) {
    
    srand(time(0)+ rand_seed);
    
    for (int i = 0; i < NUM_PROCESSES; i++){
            sem_wait(sem);
            table->allocation[i][rd_num] = 0;
            table->request[i][rd_num] = 0;
            table->maximum[i][rd_num] = (rand () %(num_resources)) + 1; //maximum claim for this process is between 1 and  num_resources
            sem_post(sem);
    }

    sem_wait(sem);
    table->available[rd_num] = num_resources;
    sem_post(sem);
    
    return 0;
}

//initializes the entire rd block in shared memeory
int init_rd_table(){
    
     //create & initialize semaphore
    if ((sem = sem_open(SEM_NAME, O_CREAT, 0644, 1)) == SEM_FAILED){
        perror("Failed to create semaphore");
        sem_unlink(SEM_NAME);
        return -1;
    }

    // Initialize shared memory for rd
    int size = sizeof(resource_descriptor);
    
    shmid = shmget ( SHMKEY, size, IPC_CREAT | IPC_EXCL | 0666 );
    
    if ( shmid == -1 ){
        perror("Error in shmget init rd_table");
        return -1; 
    }

    table = ( resource_descriptor * )(shmat( shmid, 0, 0));
    if (table == (resource_descriptor *)(-1)) {
        perror("shmat");
        return -1;
    }

    
    int percent_shareable = (rand() % (25 - 15 + 1)) + 15; //between 15 and 25 percent.
    int num_shareable = NUM_RESOURCES*(percent_shareable/100);
    
    for (int i = 0; i < NUM_RESOURCES; i++) {
        if (i <= num_shareable){
            table->type[i] = SHAREABLE;
        } else {
            table->type[i] = UNSHAREABLE;
        }
    }
    
    return 0;
}
int cleanup_rd(){
    // shared mem
    shmdt(table);
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
        perror("shmctl");

    //file semaphore
    sem_close(sem);
    sem_unlink(SEM_NAME);

    return 0;

}

