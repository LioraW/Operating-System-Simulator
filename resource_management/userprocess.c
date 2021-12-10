#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <string.h>
#include <stdbool.h>
#include "messenger.h"
#include "config.h"
#include "rd_table.h"

#define msg1 1234
#define msg2 4321

//globals
int resources[NUM_RESOURCES]; //to hold resource data
int waiting_id = -1; //resource id of the resource  the process is waiting on
int waiting_num = -1; // num instances process is waiting on

int sim_pid = -1; //global to be changed by argument

int msgid_u2o, msgid_o2u;    //to hold the message queue id
char  exec_name[100];        // for printerror

//function headers
int rand_amount(int upper, int lower);
int init_msg_queues(int * msgid1, int * msgid2);
void printerror(char * msg) { fprintf(stderr, "%s:", exec_name); fflush(stdout); perror(msg); }
int update_resource_data(int sim_pid, contents outgoing, int response);
int terminate(int sim_pid);
int get_resource_to_request(int * resource_id, int * num_instances);
int get_resource_to_release(int * rd_id, int * num);
bool is_holding_resources();

int main(int argc, char ** argv){
    
    strcpy(exec_name, argv[0]);  //get exec name

    srand(time( 0 )+ getpid()); //seed random generator

    sim_pid = atoi(argv[1]); // get the simulated pid from command line argument
    int bound = atoi(argv[2]); // get bound for time to request or release

    //initialize message queues
    if (init_msg_queues( &msgid_o2u, &msgid_u2o)== -1) {
        return -1;
    }

    //init resource data
    for (int i = 0; i < NUM_RESOURCES; i++){
        resources[i] = 0;
    }
    
    /***************************************************************************/
    while (1) {
        
       //to hold messages
        contents signal;
        contents outgoing = { 0,0,0,0, 0 };

        //will only get sent a message to wake up if it's on the regular queue. if it's waiting, it will continue to loop
        signal = get_message(msgid_o2u, sim_pid, IPC_NOWAIT);
        
        if (signal.timeused > 0){    
            int timepassed = rand_amount(bound, 0);

            //randomly terminate
            if (rand_amount(100, 0) % 10 == 0){ //randomly terminate (10% of the time)
                terminate(sim_pid);
                return 0;
            }
        
            /* If not terminating, request or release some resources */

                                        
            if (waiting_id != -1){                                                    // if we are waiting on resources, request those
                outgoing.resource_id = waiting_id;
                outgoing.requesting = waiting_num;
            
            } else if (!is_holding_resources())  {                                    //if we are not holding a resource, request resources
                 get_resource_to_request(&outgoing.resource_id, &outgoing.requesting);
            
            } else if ( rand_amount(100, 0) % 2 == 0){                               //otherwise, randomly request or release resources
                get_resource_to_release(&outgoing.resource_id, &outgoing.releasing);
            } else {
                get_resource_to_request(&outgoing.resource_id, &outgoing.requesting);
           }
            
            //a random amount of time has passed (between 0 and B)
            outgoing.timeused = timepassed;
            send_message(msgid_u2o, sim_pid, outgoing);
            
            //see what result is
            signal = get_message(msgid_o2u, sim_pid, 0);
            
            update_resource_data(sim_pid, outgoing, signal.timeused);
        
        } else {
            if (signal.terminated){ //if oss tells the program to terminate
               break;
            }
        }
        fprintf(stderr, "_ ");
    }
    return 0;
}
bool is_holding_resources(){
    for (int i = 0; i < NUM_RESOURCES; i++){
        if (resources[i] > 0){
            return true;
        }
    }
    return false;
}

int get_resource_to_request(int * resource_id, int * num_instances){
    contents signal;
    contents outgoing = { 0,0,0,0, 0 };
    
    //signal that we are going to ask for resources and then get back the max we can request
    outgoing.resource_id  = rand_amount(NUM_RESOURCES, 0);
    outgoing.requesting = 1; 
    send_message(msgid_u2o, sim_pid, outgoing);
    signal = get_message(msgid_o2u, sim_pid, 0);
    
    *num_instances = rand_amount(signal.requesting - resources[outgoing.resource_id], 1); //request a random amount that is less than (max_claims - allocation)
    *resource_id = outgoing.resource_id;
   // fprintf(stderr, "P%d is requesting %d of %d\n", sim_pid, outgoing.resource_id, outgoing.requesting);

    return 0;
}

int get_resource_to_release(int * rd_id, int * num_instances){
    //get a random resource to release (could be holding zero...)
    int id = rand_amount(NUM_RESOURCES, 0); //check a random resource
    int num = rand_amount(resources[id], 0); //relase a random amount between 0 and the amount the process is holding
    
    *rd_id = id;
    *num_instances = num;
    if (num == 0)
        return -1;
    return 0;

}

int terminate(int sim_pid){
    contents outgoing = {0,0,0,0,0};
    outgoing.terminated = 1;
    outgoing.timeused = 1000000 + rand_amount(250, 0); //make sure it has run for at least a second and then terminate at a random time
    send_message(msgid_u2o, sim_pid, outgoing); //send a message that we are terminating
    
    //send a message for each of the resources saying how many we are releasing
    for (int i = 0; i < NUM_RESOURCES; i++) {
        outgoing.resource_id = i;
        outgoing.releasing = resources[i];
        outgoing.timeused = 0;
        send_message(msgid_u2o, sim_pid, outgoing);//send OSS the message, releasing all of the resources
    }

    return 0;
}
int update_resource_data(int sim_pid, contents outgoing, int response){
     if (!response){ // 0 = OKAY
        resources[outgoing.resource_id] += (outgoing.requesting - outgoing.releasing); // either requesting or releasing will be zero
        waiting_id = -1;
        waiting_num = -1;
    } else {
        waiting_id = outgoing.resource_id;
        waiting_num = outgoing.requesting;
   }
    
    return 0;
}

int rand_amount(int upper, int lower ) {
    return (upper <= 0) ? 0 : (rand() % (upper - lower + 1)) + lower;
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


