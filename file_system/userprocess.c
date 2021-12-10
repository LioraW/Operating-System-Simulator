#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <string.h>
#include "messenger.h"
#include "clock.h"
#include "config.h"

#define MAX 1024
#define msg1 1234
#define msg2 4321

char  exec_name[100];
int random_amount(int upper, int lower);
int init_msg_queues(int * msgid1, int * msgid2);

void print_error(char * msg){
    fprintf(stderr, "[%ld]:", (long)getpid());
    fflush(stdout);
    perror(msg);
}

int main(int argc, char ** argv){
    int msgid_u2o, msgid_o2u;    //to hold the message queue id
    unsigned int total_access_time = 0; //keep track of time used to access memory

    strcpy(exec_name, argv[0]);  //get exec name

    srand(time( 0 )+ getpid()); //seed random generator

    int sim_pid = atoi(argv[1]); // get the simulated pid from command line argument 

    //initialize message queues
    if (init_msg_queues( &msgid_o2u, &msgid_u2o)== -1) {
        return -1;
    }
    
    //to hold messages
    contents signal, result;
    contents outgoing = { 0, 0,0,0,0,0};
    int num_mem_refs = 0;
    int random_term_amount =  1000 + ((random_amount(1,0) == 0) ? -1 : 1)*100;
    while (1) {
        //Get signal
         signal = get_message(msgid_o2u, sim_pid, 0);
        
        if (signal.can_start) {
            
            //request a page randomly and read or write
            // there is a small probability (configurable in config.h) that the process will request an invalid address. 
            // most of the time it will request a valid address.
            outgoing.address_request = (random_amount(100, 0) < PROB_INVALID_REQUEST)  ? NUM_PAGES*PAGE_SIZE :  random_amount((NUM_PAGES*PAGE_SIZE) - 1, 0); 
            
            outgoing.readwrite = (random_amount(100, 0) < PERCENT_READS) ? 0 : 1;  //randomly read or write, based on the configurable ( in config.h ) percentage of reads v. writes
            
            outgoing.terminated = ( num_mem_refs > random_term_amount) ? 1 : 0; //randomly terminate
            
            outgoing.timeused = total_access_time;

            //Send a message and wait for a response
            send_message(msgid_u2o, sim_pid, outgoing);
            
            result = get_message(msgid_o2u, sim_pid, 0); //it will wait for a response from OSS. if the page in not in memory, the process will wait.
            
            total_access_time += result.timeused;

        } else {
            fprintf(stderr, "singal unrecongnized, signal was %d \n", signal.can_start);
            break;
        }
    
        if (outgoing.terminated == 1 || result.terminated == 1) { //whether we decided or OSS told us to, terminate
            break; 
        }
    }
    return 0;
}

int random_amount(int upper, int lower) {
     return (rand() % (upper - lower + 1)) + lower;
}

//modifys the msgids
int init_msg_queues(int * msgid_o2u, int * msgid_u2o) {
    //initialize oss to userprocess message queue
    if (( *msgid_o2u = init_msg_queue(msg1)) == -1) {
        print_error("oss to up message queue");
        return -1;
    }
    //initialize userprocess to oss message queue
    if (( *msgid_u2o = init_msg_queue(msg2)) == -1) {
        print_error("userprocess to oss message queue");
        return -1;
    }
    
    return 0;
}


