/*
 Project 5  Deadlock
 Liora Wachsstock
 Due: November 17, 2021
*/
#include "oss.h"              //has all include files, function prototypes, and constants

pid_t childpid;               //childpid for termination process

//GLOBALS
char * filename;
int sec;
int verbose = 0;
int msgid_u2o, msgid_o2u;    //two message queues, one from userprocess to oss, and vice versa
int pid_used[NUM_PROCESSES]; //for holding how many pids are being used
char exec_name[100];         //executable name


//Accounting
int numLines = 0;
int num_forked = 0;
int num_granted_immediately = 0;
int num_waited = 0;
int times_deadlock_dect_run = 0;
int num_term_successfully = 0;
int num_term_from_deadlock = 0;
int num_in_deadlock = 0;

// process queues
Queue * pqueue;
Queue * wait_queue;


int main(int argc, char ** argv){ 
    
    strcpy(exec_name, argv[0]); //get exec name
    pid_t wpid;                 //for waiting for children
    unsigned int s, n;          //for holding time
    int sim_pid = -1;           // to hold current simulated pid
    int r, s2;                  //to hold random unblocking event time}

    
    //set up. Modifies most of the global variables above.
    if (setup(argc, argv) != 0) {
        termination(0);
        return -1;
    }
    
    //initialize bitmap that none of them are used
    for (int i = 0; i < NUM_PROCESSES; i++) {
        pid_used[i] = 0;
    }
    
    //initialize resource_descriptor with a number of instances
    for (int i = 0; i < NUM_RESOURCES; i++) {
        init_rd(i, (rand() % 10) + 1, i);
    }

    //check if there are any pids that haven't been used yet
    int open = 0;
    
    // read start time
    readclock(&s, &n);
    unsigned int lastGenTimeS = s;
    unsigned int lastGenTimeN = n;
    setUnblockEventtime(s, n, &r, &s2);
    
    //loop until MAX_PROCESSES have been forked
    while(1){
        //in the begining there are no processes forked, so increment the clock until it is time to fork
        if (num_forked >= MAX_PROCESSES) {
            fprintf(stderr, "Max Processes forked.\n");
            break;
        }
        if (num_forked <= 0){
            updateclock(1, 0);
        }

        //generate a process every one second on the logical clock
        if (timeForNewFork(lastGenTimeS, lastGenTimeN)){
            for (int i = 0; i < NUM_PROCESSES; i++ ) {
                if (pid_used[i] == 0 ) {
                    sim_pid = i;            // save pid
                    pid_used[i] = 1;        // update pid table 
                    open = 1;
                    num_forked++;           //track number of forked processes
                     break;
                }
            }
        
        //based on that pid, fork a process or not
            if (open){
                // branch off into child or parent code
                switch ( childpid = fork() )
                {      
                    case -1:
                        print_error("Problem with fork");
                        termination(0);
                        return -1;
                    case 0:
                        /* child */
                        child(sim_pid);
                        return 0;
                    default: 
                        lastGenTimeS = s;
                        lastGenTimeN = n;
                        enqueue(pqueue, sim_pid); //put the process on the queue
                        break;
                }
            }
        } 
        
        schedule(pqueue); //this is where the magic happens
        
        readclock(&s, &n);
        open = 0;

        //wait for children...
        while((( wpid = waitpid(-1, NULL, WNOHANG)) > 0) && (errno != EINTR));

        updateclock(1, rand() % 1000); //overhead for scheduling processes
   
   } //inifinate loop

    //wait for  remainig children...
    while((( wpid = waitpid(-1, NULL, WNOHANG)) > 0) && (errno != EINTR)){
        sleep(2);
    }

    logstats();

    termination(0);

    return 0;
}
int schedule(Queue * queue){
    contents outgoing = { 0,0,0,0,0};
    contents result = { 0,0,0,0,0};
    
    int sim_pid = dequeue(pqueue);
    
    if ( sim_pid != -1 ){
        outgoing.timeused = 0;                        //used as a signal
        
        send_message(msgid_o2u, sim_pid, outgoing);   //send messange telling them we are ready
        
        result = get_message(msgid_u2o, sim_pid, 0);  //wait for process to send back at least one result
        
        switch( process_result(sim_pid, result)) {    
           case -1:                                   //process terminated
                num_term_successfully++;
                userprocess_termination(sim_pid);
                break;
            case 0:                                   //result OK
                enqueue(pqueue, sim_pid);
                break;
            case 1:                                   //result not OK
                enqueue(wait_queue, sim_pid);
                break;
            default:
                break;
        }
    }
        return 0;
}

int process_result(int sim_pid, contents result){
    int return_val = -1;
    if (result.terminated){
        return_val =  -1; 
    } else if (result.releasing) {
        return_val =  process_release(sim_pid, result);
    }  else if (result.requesting) {
        return_val = process_request(sim_pid, result);
    } else {
        return_val = 0;
    }
    return return_val;
}

int process_release(int sim_pid, contents result){
    unsigned int s,n;
    char buf[BUF_SIZE];
    int dispatchtime = rand() % 2;

    contents content = { 0,0,0,0,0};
    int num = (result.releasing > getelement(result.resource_id, sim_pid, MAX)) ? getelement(result.resource_id, sim_pid, MAX) : result.releasing; //it cannot release more than max claim
    
    if ( modify_rd(result.resource_id, sim_pid, ALLOC, -num) != -1 && modify_rd(result.resource_id, sim_pid, AVAIL, num) != -1 ){
        readclock(&s, &n);
        
        //log
        buf[0] = '\0';
        snprintf(buf, sizeof(buf), "Master has acknowledged P%d releasing %d instances of R%d at time %d.%d\n ", sim_pid, num, result.resource_id, s,n);
        logmsg(buf, filename);
        
        //WAKE UP PROCESSES 
        int num_checked = 0;
        while (!queue_is_empty(wait_queue) && num_checked < NUM_PROCESSES){
            int temp_pid = dequeue(wait_queue);
            if (getelement(result.resource_id, temp_pid, REQ) > 0){
                enqueue(pqueue, temp_pid); //it was waiting on that resource, so put it back on the rotation
            } else {
                enqueue(wait_queue, temp_pid); //if not  put it back
            }
            num_checked++;
        }
        
        content.timeused = 0; //set bit saying release was OKAY
    
    } else {
        content.timeused = 1; //set bit saying release was NOT okay
    }
    
    send_message(msgid_o2u, sim_pid, content); //send response to userprocess

    if (result.timeused != -1) {
        //update system time
        updateclock(0, result.timeused + dispatchtime);
        readclock(&s, &n);
    }

    return content.timeused; //1 for not okay, 0 for okay

}
int process_request(int sim_pid, contents result){
    unsigned int s,n, dispatchtime = 0;
    char buf[BUF_SIZE];
    dispatchtime = rand() % 2;
    int num_deadlocked = 0;
    contents content = { 0,0,0,0,0};
    contents result2;
    
    //tell userprocess the max they can request
    content.requesting = getelement(result.resource_id, sim_pid, MAX);
    send_message(msgid_o2u, sim_pid, content); 

     //get how many they are requesting
    result2 = get_message(msgid_u2o, sim_pid, 0); 
    
    if (result2.requesting > 0){
        readclock(&s, &n);
        
        if (verbose){
            //log
            buf[0] = '\0';
            snprintf(buf, sizeof(buf), "Master had detected P%d requesting %d:%d at time %d.%d ", sim_pid, result2.resource_id, result2.requesting, s,n);
            logmsg(buf, filename);
        
            buf[0] = '\0';
            snprintf(buf, sizeof(buf), "\n\nMaster Running Deadlock detection at time %d:%d\n",s,n);
            logmsg(buf, filename);
        }

        //add the resource it wants to the request matrix
        modify_rd(result2.resource_id, sim_pid, REQ, result2.requesting);     
    
        //Running Deadlock Detection algorithm
        times_deadlock_dect_run++;

        if (!deadlock_avoidance(sim_pid, &num_deadlocked)){  
            /*** NO DEADLOCK DETECTED **/
            if (verbose){
                //log
                buf[0] = '\0';
                snprintf(buf, sizeof(buf), "    Safe state after granting request\n");
                logmsg(buf, filename);
            }

            //update rd
            modify_rd(result2.resource_id, sim_pid, ALLOC, result2.requesting);
            modify_rd(result2.resource_id, sim_pid, AVAIL, -result2.requesting);
            modify_rd(result2.resource_id, sim_pid, REQ, -result2.requesting);

            //log
            readclock(&s, &n);
            buf[0] = '\0';
            snprintf(buf, sizeof(buf), "Master granting P%d %d instances of R%d at time %d:%d\n ", sim_pid, result2.requesting, result2.resource_id, s, n);
            logmsg(buf, filename);
            num_granted_immediately++;

            content.timeused = 0; //set bit telling userporcess request was OKAY     
        } else {
            /*** DEADLOCK DETECTED ***/
            readclock(&s, &n);
            buf[0] = '\0';
            snprintf(buf, sizeof(buf), "    Unsafe state after granting request; request not granted.\n    P%d added to wait queue, waiting for R%d:%d\n ", 
                                            sim_pid, result2.resource_id, result2.requesting);
            logmsg(buf, filename);
            num_waited++;
            num_in_deadlock += num_deadlocked;

            //deadlock resolving. terminate all process that have that requested resource except for the one that requested it
            for (int i = 0; i < NUM_PROCESSES; i++){
                if (getelement(result2.resource_id, i, ALLOC) > 0 && i != sim_pid){
                    tell_process_to_terminate(i);
                    num_term_from_deadlock++;
                }
            }
            content.timeused = 1; //set bit telling process that request was NOT okay, and will return this value to tell process_result  to put it on wait queue
            
       }
        //log allocation every 20 requests
        if ((num_waited + num_granted_immediately) % 20 == 0){
            log_available_resources();
            log_matrix(ALLOC);
        }
    } else {
        //its requesting 0, fine, but we did nothing
        content.timeused = 0;
    }
    
    send_message(msgid_o2u, sim_pid, content); //send response to userprocess

    if (result.timeused != -1) {
        //update system time
        updateclock(0, result.timeused + dispatchtime);
        readclock(&s, &n);
    }

    return content.timeused; //1 for not okay, 0 for okay
}
int userprocess_termination(int sim_pid){
    char buf[BUF_SIZE];
    if (verbose){
        //log
        buf[0] = '\0';
        snprintf(buf, sizeof(buf), "Master: Process %d terminated\n", sim_pid);
        logmsg(buf, filename);
    }
    contents result;

    //Get messges for each of the processes
    for (int i = 0; i < NUM_RESOURCES; i++){
        result = get_message(msgid_u2o, sim_pid, 0);
        if (result.releasing){
            process_result(sim_pid, result);
        }
    }

    pid_used[sim_pid] = 0; //set the pid as avalible

    //remove the pid from the queue
    int numchecked = 0;
    while(!queue_is_empty(pqueue) && numchecked < NUM_PROCESSES){
        int temp = dequeue(pqueue);
        
        if (temp != sim_pid){
            enqueue(pqueue, temp); //put the ones that are not this pid back
        }
        numchecked++;
    }

    numchecked = 0;
    while(!queue_is_empty(wait_queue) && numchecked < NUM_PROCESSES){
        int temp = dequeue(wait_queue);
        if (temp != sim_pid){
            enqueue(wait_queue, temp); //put this ones that are not this pid back
        }
        numchecked++;
    }
    return 0;

}

int tell_process_to_terminate(int sim_pid){
    contents content = {0,0,0,1,1}; //signal that they cannot start and they should terminate
    contents response;

    send_message(msgid_o2u, sim_pid, content);
    
    response = get_message(msgid_u2o, sim_pid, 0);

    if (response.terminated == 1){
        userprocess_termination(sim_pid);
    } else {
        fprintf(stderr, "Tried to terminate %d but wrong response\n", sim_pid);
    }
    return 0;

}

bool timeForNewFork(unsigned int lastGenTimeS, unsigned int lastGenTimeN) {
    unsigned int s, n;
    readclock(&s, &n);
    return  (s > (lastGenTimeS + (rand() % maxTimeBetweenNewProcsSecs))) 
        && (n > (lastGenTimeN + (rand() % maxTimeBetweenNewProcsNS)));
}

void setUnblockEventtime(int currentSec, int currentNS, int * futureSec, int * futureNS) {
    *futureSec = currentSec + (rand() % 5);
    *futureNS = currentNS + (rand() % 1000);
}

int child(int sim_pid) {
    char buf1[BUF_SIZE];
    char buf2[BUF_SIZE];

    snprintf(buf1, sizeof(buf1), "%d", sim_pid);
    snprintf(buf2, sizeof(buf2), "%d", BOUND); 
 
    execl("./userprocess", "userprocess", buf1, buf2,  NULL);
    perror("execl");

    return -1;
}
void log_available_resources(){    
    char overallbuf[BUF_SIZE];
    char tempbuf[BUF_SIZE];

    tempbuf[0] = '\0';
    overallbuf[0] = '\0';

    strcat(overallbuf, "Available Resources:\n");
    for (int j = 0; j < NUM_RESOURCES; j++){
        snprintf(tempbuf, sizeof(tempbuf), "R%d:%d  ",j, getelement(j, 0, AVAIL)); //resource numbers and avaialablility
        strcat(overallbuf, tempbuf);
        tempbuf[0] = '\0';
    }
    logmsg(overallbuf, filename);
}


int log_matrix(int matrix_symbol){
    int num;
    char overallbuf[BUF_SIZE];
    char tempbuf[BUF_SIZE];
    
    tempbuf[0] = '\0';
    overallbuf[0] = '\0';

    strcat(overallbuf, "    ");
    for (int j = 0; j < NUM_RESOURCES; j++){
        snprintf(tempbuf, sizeof(tempbuf), "R%d  ",j); //resource numbers
        strcat(overallbuf, tempbuf);
        tempbuf[0] = '\0';
    }
    logmsg(overallbuf, filename);
    overallbuf[0] = '\0';
    
    strcat(overallbuf,  "\n");
    for (int i = 0; i < NUM_PROCESSES; i++){
        snprintf(tempbuf, sizeof(tempbuf), "P%d  ", i); //process numbers
        strcat(overallbuf, tempbuf);
        tempbuf[0] = '\0';
        
        if(i < 10) {
                // add another space for double digits
                strcat(overallbuf, " ");
                tempbuf[0] = '\0';
        }

        for (int j = 0; j < NUM_RESOURCES; j++){

            num = getelement(j, i, matrix_symbol); //value of item(i,j) of the matrix to be displayed

            snprintf(tempbuf, sizeof(tempbuf), "%d   ", num);
            strcat(overallbuf, tempbuf);
            tempbuf[0] = '\0';

            if(j >= 10) {
                // add another space for double digits
                strcat(overallbuf, " ");
                tempbuf[0] = '\0';
            }
        }
        strcat(overallbuf, "\n");
        
        logmsg(overallbuf, filename); //log each process line
        overallbuf[0] = '\0';

    }

    return 0;
}
void logstats(){
    char buf[BUF_SIZE];
    buf[0] = '\0';
    snprintf(buf, sizeof(buf), "Requests Granted immediately: %d\nRequests granted after waiting a bit: %d\n", num_granted_immediately, num_waited);
    logmsg(buf, filename);
    
    buf[0] = '\0';
    snprintf(buf, sizeof(buf), "Number processes terminated early: %d\nNumber processes terminated successfully: %d\n ", num_term_from_deadlock, num_term_successfully);
    logmsg(buf, filename);
    
    buf[0] = '\0';
    snprintf(buf, sizeof(buf), "Number of times the deadlock avoidance algorithm was run: %d ",times_deadlock_dect_run);
    logmsg(buf, filename);
    
    double term_av = 0.0;
    if (num_in_deadlock != 0){
        term_av = (num_term_from_deadlock/num_in_deadlock)*100.00;
    }

    buf[0] = '\0';
    snprintf(buf, sizeof(buf), "Percentage of processes deadlocked terminated: %0.2f\n ", term_av);
    logmsg(buf, filename);


}
// Log messages in logfile
int logmsg(char * msg, const char * fname) {
    fprintf(stderr, "\n%s", msg);

    if (numLines <= 10000) {
        FILE *fileptr = fopen(fname, "a+");

        if (fileptr){
            fputs(msg, fileptr);
            numLines++;
            if (numLines == 1000) {
                fputs("End of file, too many lines", fileptr);
            }
            fclose(fileptr);
        } else {
            perror("logmsg");
        }
    } 
    return 0;
}

int setup ( int argc, char ** argv) {
    if ((filename = malloc(BUF_SIZE*sizeof(char))) ==  NULL) {
        perror("Malloc in main oss");
        return -1;
    }
    if (validatearg(argc, argv, filename, &sec) != 0){
        print_error("problem in validate args");
        return -1;
    }
    setup_intr_handlers();
    alarm(sec);

    if (initclock() == -1) {
        perror("error in initilizing clock");
        return -1;
    }
    if (init_msg_queues(&msgid_o2u, &msgid_u2o) == -1) {
        return -1;
    }
    if (init_rd_table() == -1) {
        print_error("Problem with int_rd_table");
        return -1;
    }
    if ( (pqueue = initqueue()) == NULL) {
        return -1;
    }
    if ( (wait_queue= initqueue()) == NULL) {
        return -1;
    }

    srand(time(0) + getpid());
    
    // Clean the slate
    fprintf(stderr, "Deleting previous logfile, if it exists...\n");
    char buf[100];
    snprintf(buf, sizeof(buf), "rm -f %s", filename);
    system(buf);

    return 0;
}
void setup_intr_handlers(){
 //setup SIGINT handler ( from textbook pg. 269 )
    struct sigaction act;
    act.sa_handler = sigint_handler;
    act.sa_flags = 0;
    if ((sigemptyset ( &act.sa_mask) == -1 ) ||
        ( sigaction(SIGINT, &act, NULL) == -1 )){
            print_error("Failed to set SIGINT to handle Ctrl-C");
    }

    //setup SIGALARM handler
    struct sigaction alarmact;
    alarmact.sa_handler = sigalarm_handler;
    alarmact.sa_flags = 0;
    if ((sigemptyset ( &alarmact.sa_mask) == -1 ) ||
        ( sigaction(SIGALRM, &alarmact, NULL) == -1 )){
            print_error("Failed to set SIGALARM to go off after the specified time");
    }

}

void termination(int early){
    if (childpid != 0){
        contents outgoing = {0,0,0,1,1}; //termination signal to all children
        for (int i = 0; i < NUM_PROCESSES; i++){
            if (pid_used[i] == 1){
                send_message(msgid_o2u, i,outgoing);
            }
        }
                
        delete_queue(pqueue);
        delete_queue(wait_queue);
        cleanupclock();
        cleanup_rd();
        rem_msg_queue(msgid_u2o);
        rem_msg_queue(msgid_o2u);
        

    }

  // Kill children if early termination
    if (early){
        logstats();
        char failmsg[] = "Failed to kill children\n";
        int failmsglen = sizeof(failmsg);
        if ( (kill(0, SIGTERM)) != 0){
            write(STDERR_FILENO, failmsg, failmsglen);
        }
    }
}

// set ctrl c inturrupt handler
void  sigint_handler(int signo){
    char msg[] = "\n***Caught ctrl C***\n";
    int msglen = sizeof(msg);
    write(STDERR_FILENO, msg, msglen);

    termination(1);
}
// set timer
void sigalarm_handler(int signo) {
    char msg[] = "***Time's up!***\n";
    int msglen = sizeof(msg);
    write(STDERR_FILENO, msg, msglen);

    termination(1);
}

int init_msg_queues(int * msgid1, int * msgid2) {
    //initialize message queue(s)
    if ((*msgid2 = init_msg_queue(msg2)) == -1) {
        print_error("up to oss message queue");
        return -1;
    }

    if ((*msgid1 = init_msg_queue(msg1)) == -1) {
        print_error("oss to up message queue");
        return -1;
    }
    return 0;
}

//return -1 if there is a problem with the input
int validatearg(int argnum, char ** argv, char * filename, int * sec){
    *sec = MAX_TIME; //set default in case time is not supplied
    int number, opt;

    //in case the user does not supply a file name, use "logfile"
    strcpy(filename, "logfile");

    while ((opt = getopt(argnum, argv, "s:l:hv")) != -1){
        switch(opt){
            case 'h':
                printf("This program simulates an operating system that allocates resources to processes and prevents/detects/removes deadlocks.\n");
                printf("You can run it by ./oss\n");
                printf("You can also run it with -t [seconds], which will terminate the program after [seconds] seconds.\n");
                printf("Running the program this program with -h prints this message then terminates.\n");
                printf("Running the program with -v puts it in verbose mode, which prints more messages about allocation, request, and release.\n");
                return -1;

            case 'v':
                verbose = 1;
                break;
            case 's':
                //check if arg is a number
                number = strtol(optarg, &optarg, 10);
                if (*optarg != '\0') {
                    errno=1;
                    print_error("The argument supplied is not a number");
                    return -1;
                }else {
                    //take the number
                    *sec = number;
                }
                break;
            
            case 'l':
                strcpy(filename, optarg);
                break;
            case ':':
                errno=1;
                print_error( "Option needs a value");
                return -1;

            case '?':
                errno=1;
                print_error("Unrecongnized option");
                return -1;
        }
    }
    //Deal with extra arguments
    for (; optind < argnum; optind++){
            printf("This is an extra argument: %s\n", argv[optind]);
    }
   return 0;
}
