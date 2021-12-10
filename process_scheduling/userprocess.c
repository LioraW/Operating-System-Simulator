#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <string.h>
#include "messenger.h"

#define MAX 1024
#define msg1 1234
#define msg2 4321
/*
typedef struct message_contents {
    int timeused;
    int timeslice;
    int terminated;
    int can_start;
} contents;
*/
enum { IO_bound = 0, CPU_bound };

char  exec_name[100];
int random_amount(int number);
int init_msg_queues(int * msgid1, int * msgid2);
void  printerror(char * msg) { fprintf(stderr, "%s:", exec_name); fflush(stdout); perror(msg); }

int main(int argc, char ** argv){
    int msgid_u2o, msgid_o2u;    //to hold the message queue id
    
    strcpy(exec_name, argv[0]);  //get exec name

    srand(time( 0 )+ getpid()); //seed random generator

    int sim_pid = atoi(argv[1]); // get the simulated pid from command line argument
    int processType = atoi(argv[2]); // 0 for I/O, 1 for CPU bound 

    //initialize message queues
    if (init_msg_queues( &msgid_o2u, &msgid_u2o)== -1) {
        return -1;
    }
    
    //to hold messages
    contents signal;
    contents outgoing = { 0,0,0,0,0};
    int ready, terminating;

    while (1) {
        //Get signal
         signal = get_message(msgid_o2u, sim_pid, 0);
        
        if (signal.can_start) {
            //Decide what it will do
            ready  = random_amount(100);
            terminating = random_amount(30);

            // Determine outcome
            if ( processType == CPU_bound  && ( ready  % 2 != 0) && ( ready % 3 == 0 )) { //block less often if it is a CPU bound process (specifically, around 17% of the time)
                outgoing.blocked = 1;        
            
            } else if ( ready %  2 != 0) {                                      //block more often if it is a I/O bound process (around %50 of the time)
                outgoing.blocked = 1;
                outgoing.usedIO = 1;
            }else {
                outgoing.timeused = random_amount(8);                           //if the random number is odd, use a certain amount of its time.
            
            }
           if (( terminating  % 2 != 0) ){                                      //randomly terminate
                outgoing.terminated = 1;
            }

            send_message(msgid_u2o, sim_pid, outgoing);                         //send OSS the message

        } else {
            fprintf(stderr, "singal unrecongnized, signal was %d \n", signal.can_start);
            break;
        }
    
        if (outgoing.terminated == 1) {                                         //if it sent a message that its terminating, terminate. 
            break; 
        }
    }
    return 0;
}

int random_amount(int number) {
     return (rand() % ((2*number) + 1));
}

//modifys the msgids
int init_msg_queues(int * msgid_o2u, int * msgid_u2o) {
    //initialize oss to userprocess message queue
    if (( *msgid_o2u = init_msg_queue(msg1)) == -1) {
        printerror("oss to up message queue");
        return -1;
    }
    //initialize userprocess to oss message queue
    if (( *msgid_u2o = init_msg_queue(msg2)) == -1) {
        printerror("userprocess to oss message queue");
        return -1;
    }
    
    return 0;
}


