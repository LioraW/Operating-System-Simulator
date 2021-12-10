#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include "pcb_table.h"

#define MSG_BUF_SIZE 1024*sizeof(char)
#define SHMKEY 613614
#define SEM_NAME "tablesem"

int shmid;
char * paddr;
sem_t * sem;

process_control_block * table; // global table

// modify a specific element of a pcb using enum for pcb elements
int modifypcb(int pid, int elem, double newdval, int newival){

    if(pid > NUM_PROCESSES ||  pid < 0){
        fprintf(stderr, "modify pcb: pid does not exsist.\n");
    } else {

        sem_wait(sem);
        
        switch(elem)
        {
            case CPU:
                setcputimeused(&(table[pid]), newdval);
                break;
            case SYS:
                setsystimeused(&(table[pid]), newdval);
                break;
            case BURST:
                setbursttime(&(table[pid]), newdval);
                break;
            case PP:
                setpp(&(table[pid]), newival);
                break;
            case LAST_TIME:
                settimeoflastrun(&(table[pid]), newdval);
            default:
                //fprintf(stderr, "cannot modify process with sim_pid %d\n", pid);
                break;
        }
        sem_post(sem);
    }
        return 0;
}

// get a specific element of the pcb using enum for pcb elements
double getelement(int pid, int elem){
    
    double result;
    if(pid > NUM_PROCESSES ||  pid < 0){
        fprintf(stderr, "get element of pcb: pid %d does not exsist.\n", pid);
    } else {

        sem_wait(sem);

        switch(elem)
        {
            case CPU:
                result = getcputimeused(table[pid]);
                break;
            case SYS:
                result =  getsystimeused(table[pid]);
                break;
            case BURST:
                result = getbursttime(table[pid]);
                break;
            case PP:
                result =  (double)getprocesspriority(table[pid]);
                break;
            case LAST_TIME:
                result = gettimeoflastrun(table[pid]);
            default:
                //fprintf(stderr, "cannot get element %d for process with sim_pid %d\n", elem, pid);
                result =  -1.0;
                break;
        }
        sem_post(sem);
    }
        return result;
}

//getters and setters for pcb itself. all need to be protected by semaphores!
double getcputimeused(process_control_block pcb){
        return pcb.cpu_time_used;
}
double getsystimeused(process_control_block  pcb){
        return pcb.total_system_time;
}
double getbursttime(process_control_block  pcb){
        return pcb.last_burst_time;
}
int getprocesspriority(process_control_block pcb){
        return pcb.process_priority;
}
double gettimeoflastrun(process_control_block pcb) {
    return pcb.time_last_run;
}
int setcputimeused(process_control_block * pcb, double cpu_time){
    pcb->cpu_time_used += cpu_time;
    return 0;
}
int setsystimeused(process_control_block * pcb, double sys_time){
    pcb->total_system_time += sys_time;
    return 0;
}
int setbursttime(process_control_block * pcb, double burst_time){
    pcb->last_burst_time = burst_time;
    return 0;
}
int setsimpid(process_control_block * pcb, int sim_pid){
    pcb->sim_pid = sim_pid;
    return 0;
}
int setpp(process_control_block *  pcb, int pp){
    pcb->process_priority = pp;
    return 0;
}
int settimeoflastrun(process_control_block * pcb, double last_run){
    pcb->time_last_run = last_run;
    return 0;
}
// initializes a single pcb block
int initpcb(int pid, int pp) {
    process_control_block pcb = {pid, 0, 0, 0, pp, 0};
    
    sem_wait(sem);
    
    table[pid] = pcb;
    
    sem_post(sem);
    
    return 0;
}

//initializes the entire pcb block in shared memeory
int init_pcb_table(){
    
     //create & initialize semaphore
    if ((sem = sem_open(SEM_NAME, O_CREAT, 0644, 1)) == SEM_FAILED){
      perror("Failed to create semaphore for protecting file");
      sem_unlink(SEM_NAME);
    }

    // Initialize shared memory for pcb
    int size = sizeof(process_control_block)*NUM_PROCESSES;
    
    shmid = shmget ( SHMKEY, size, IPC_CREAT | IPC_EXCL | 0666 );
    
    if ( shmid == -1 ){
        perror("Error in shmget init license");
        return -1;
    }

    table  = ( process_control_block * )(shmat( shmid, 0, 0));
    if (table == (process_control_block *)(-1)) {
        perror("shmat");
        return(-1); 
    }

    return 0;
}

int cleanup_pcb(){
    // shared mem
    shmdt(table);
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
        perror("shmctl");


    //file semaphore
    sem_close(sem);
    sem_unlink(SEM_NAME);
    return 0 ;
}
