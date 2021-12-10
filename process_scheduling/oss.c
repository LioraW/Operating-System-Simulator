/*
 Project 4 Process Scheduler
 Liora Wachsstock
 Due: October 31, 2021
*/
#include "oss.h"              //has all include files, function prototypes, and constants

pid_t childpid;               //childpid for termination process

//for holding arguments
char * filename;
int sec;

//Accounting
int numLines = 0;
int num_forked = 0;
total totalsIO = { 0, 0, 0 };
total totalsCPU = { 0, 0, 0 };
int numIOlaunched = 0;
int numCPUlaunched = 0;

double totalIdleCPU = 0; // for calculating averages

Queue * highQueue;
Queue * lowQueue;
Queue * blockedQueue;

int msgid_u2o, msgid_o2u;    //two message queues, one from userprocess to oss, and vice versa
int pid_used[NUM_PROCESSES]; //for holding how many pids are being used
char exec_name[100];         //executable name

int main(int argc, char ** argv){ 
    strcpy(exec_name, argv[0]); //get exec name
    pid_t wpid;                 //for waiting for children
    unsigned int s, n;          //for holding time
    int sim_pid = -1;           // to hold current simulated pid
    int r, s2;                  //to hold random unblocking event time
    char buf[BUF_SIZE];         //to build log messages

    //set up. Modifies most of the global variables above.
    if (setup(argc, argv) != 0) {
        termination(0);
        return -1;
    }

    //initialize bitmap that none of them are used (ie -1)
    for (int i = 0; i < NUM_PROCESSES; i++) {
        pid_used[i] = 0;
    }
    
    //check if there are any pids that haven't been used yet
    int open = 0;
    
    // read start time
    readclock(&s, &n);
    unsigned int lastGenTimeS = s;
    unsigned int lastGenTimeN = n;
    setUnblockEventtime(s, n, &r, &s2);
    
    fprintf(stderr, "Running... (to show Process Control Block of each process as it runs, uncomment the first line of  logmsg() in oss.c)\n");
    //loop until MAX_PROCESSES have been forked
    while(1){
        //in the begining there are no processes forked, so increment the clock until it is time to fork
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
        }
    
        // based on that pid, fork a process or not
        if ( !open ) {
            logmsg("OSS: PID table full\n", filename); 
        } else {
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
                        initpcb(sim_pid, 0);         //initialize this process control block
                        enqueue(highQueue, sim_pid); //enqueue it in the highest priority first
                        break;
                }
        }

        //stop generating if it has forked enough 
        if (num_forked >= MAX_PROCESSES)
            break;
        
        // switch back and forth between dequeing low priority and high priority
        if ( (rand()%10) % 2 == 0 ) {   
            schedule(highQueue, 0);  
        } else {
            schedule(lowQueue, 1);
        }

        readclock(&s, &n);
        
        //if someone is in the blocked queue and the random event happened, move them to the ready queue
        if (!queue_is_empty(blockedQueue) && (s >= r || n >= s2 )) {
            
            while ( !queue_is_empty(blockedQueue) ) {
                enqueue(highQueue, dequeue(blockedQueue));
                updateclock(rand() % 2  , rand() % 2 ); //update clock with more time because it takes time to switch contexts to blocked queue
            }
            readclock(&s, &n);
            
            //log
            buf[0] = '\0';
            snprintf(buf, sizeof(buf), "OSS: Event Occured. Moved all processes on blocked queue to ready queue at time %d:%d\n", s, n);
            logmsg(buf, filename);

            setUnblockEventtime(s, n, &r, &s2);
        } 
        open = 0;
    
        //wait for children...
        while((( wpid = waitpid(-1, NULL, WNOHANG)) > 0) && (errno != EINTR));

        updateclock(1, rand() % 1000); //overhead for scheduling processes
   }

   // ************ Report ********* // 
    fprintf(stderr, "\n\n ************** REPORT ******************\n ");
    fprintf(stderr, "\n\nAverage CPU time for IO processes: %lf\n", totalsIO.cpu/numIOlaunched);
    fprintf(stderr, "Average System time for IO processes: %lf\n", totalsIO.sys/numIOlaunched);
    fprintf(stderr, "Average wait time for IO processes: %lf\n", totalsIO.wait/numIOlaunched);
    fprintf(stderr, "Average CPU time for CPU bound processes: %lf\n", totalsCPU.cpu/numCPUlaunched);
    fprintf(stderr, "Average sys time for CPU bound processes: %lf\n", totalsCPU.sys/numCPUlaunched);
    fprintf(stderr, "Average wait time for CPU bound processes: %lf\n\n", totalsCPU.wait/numCPUlaunched);
    

    // try to resolve orphan messages
    fprintf(stderr, "The program has finsihed forking processes...\n"); 
    while(!queue_is_empty(highQueue)){
            int index = dequeue(highQueue);
            get_message(msgid_u2o, index, IPC_NOWAIT);
    }
    while( !queue_is_empty(lowQueue)){
            int index = dequeue(lowQueue);
            get_message(msgid_u2o, index, IPC_NOWAIT);
    }

    //wait for  remainig children...
    while((( wpid = waitpid(-1, NULL, WNOHANG)) > 0) && (errno != EINTR)){
        sleep(2);
    }
    termination(0);

    return 0;
}    

int schedule(Queue * queue, int currentQueue){
    unsigned int s,n, dispatchtime = 0;
    int sim_pid;
    char buf[BUF_SIZE];
    
    contents content = { 0, 0,0,1};
    contents result;
    
    sim_pid = dequeue(queue);
    
    if ( sim_pid != -1 ){
        readclock(&s, &n);
        
        //log
        snprintf(buf, sizeof(buf), "OSS:Dispatching process with PID %d from queue %d  at time %d.%d\n", sim_pid, currentQueue, s, n);
        logmsg(buf, filename);

        //**** here oss and any other process is not running, waiting for this proccess to run
        send_message(msgid_o2u, sim_pid, content); //send message to userprocess telling them we are ready
        result = get_message(msgid_u2o, sim_pid, 0);  //wait for process to send back the result
        // *** at this point control returns to the oss. 
        
        dispatchtime = rand() % 2;
        
        //log
        buf[0] = '\0';
        snprintf(buf, sizeof(buf), "OSS: Total dispatch time this dispatch was %d nanoseconds\n", dispatchtime);
        logmsg(buf, filename);

        //process the result
        if (result.blocked) {
            //log
            buf[0] = '\0';
            snprintf(buf, sizeof(buf), "OSS: process with pid %d blocked, waiting for it to be unblocked...\n ", sim_pid);
            logmsg(buf, filename);
                 
        } else if (result.timeused != error) {

            //log
            buf[0] = '\0';
            snprintf(buf, sizeof(buf), "OSS: Receiving that process with PID %d ran for %d nanoseconds\n", sim_pid, result.timeused);
            logmsg(buf, filename);
        }
        
        if (result.usedIO){
            //log
            buf[0] = '\0';
            snprintf(buf, sizeof(buf), "OSS: Receiving that process with PID %d used IO\n", sim_pid);
            logmsg(buf, filename);
            modifypcb(sim_pid, PP, 0, 0);
                
        } else { //it didn't use IO, so put it in the lower priority queue
            modifypcb(sim_pid, PP, 0, 1);
        }
            
        
        //modify pcb with results
        modifypcb(sim_pid, CPU, (double)result.timeused, 0);
        modifypcb(sim_pid, SYS, (double)result.timeused + (double)dispatchtime, 0);
        modifypcb(sim_pid, BURST, (double)result.timeused, 0);
        //modifypcb(sim_pid, LAST_TIME, (double)s + (double)(n/NANOSECONDS_IN_SECOND),0); 
        
        //update system time
        updateclock(0, result.timeused + dispatchtime);
        readclock(&s, &n);
        if (result.terminated == 1){
            //log
            buf[0] = '\0';
            snprintf(buf, sizeof(buf), "OSS: Receiving that process with PID %d also terminated\n", sim_pid);
            logmsg(buf, filename);

            pid_used[sim_pid] = 0; //done with this pid
             
            //keep track for report
            logTotals(sim_pid, 0);
            if (getelement(sim_pid, PP) == 0){
                numIOlaunched++;
            }else{
                numCPUlaunched++;
            }
        
        } else if (result.timeused != blocking) { //if it didnt terminate and its not blocked, it wants to run again.
            if (getelement(sim_pid, PP) == 0) //put IO bound processes on high priority queue
                enqueue(highQueue, sim_pid);
            else
                enqueue(lowQueue, sim_pid); // put CPU bound processes on low priorty queue
        
        } else if (result.timeused == blocking) { //put the blocked process on the blocked queue
            enqueue(blockedQueue, sim_pid);

        }

    }
    else {
         //log
        buf[0] = '\0';
        snprintf(buf, sizeof(buf), "OSS: No Messages in queue %d (0 = highQueue, 1 = lowQueue)\n", currentQueue);
        logmsg(buf, filename);
    }

    return 0;
}
void logTotals(int pid, int waitTime) {
    unsigned int s, n;
    readclock(&s, &n);

    //choose which type of process to track
    total * t;
    if (getelement(pid, PP) == 0) { //IO bound
        t = &totalsIO;
    } else {
        t = &totalsCPU; //CPU bound
    }
    
    t->cpu += getelement(pid, CPU);
    t->sys += getelement(pid, SYS);
    t->wait += ( (s + (n/NANOSECONDS_IN_SECOND)) - (getelement(pid, LAST_TIME)));
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
int cpuBoundOrIOBound() {
    srand(time(0));
    int number = (rand() % 10)*100;
    if (number > percentIOBoundProcs) {
        return 1;
    } else {
        return 0;
    }
}

int child(int sim_pid) {
    char buf1[BUF_SIZE];
    char buf2[BUF_SIZE];

    snprintf(buf1, sizeof(buf1), "%d", sim_pid);
    snprintf(buf2, sizeof(buf2), "%d", cpuBoundOrIOBound() ); 
    
    execl("./userprocess", "userprocess", buf1, buf2, NULL);
    perror("execl");

    return -1;
}

// Log messages in logfile
int logmsg(char * msg, const char * fname) {
    //fprintf(stderr, "\n%s", msg);
    
    if (numLines <= 1000) {
        FILE *fileptr = fopen(fname, "a+");

        if (fileptr){
            fputs(msg, fileptr);
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
    
    if (init_pcb_table() == -1) {
        print_error("Problem with initpcb");
        return -1;
    }

    if ( (highQueue = initqueue()) == NULL) {
        return -1;
    }
    if ( (lowQueue = initqueue()) == NULL) {
        return -1;
    }
    if ( (blockedQueue = initqueue() ) == NULL) {
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
        delete_queue(highQueue);
        delete_queue(lowQueue);
        cleanupclock();
        cleanup_pcb();
        rem_msg_queue(msgid_u2o);
        rem_msg_queue(msgid_o2u);

    }

  // Kill children if early termination
    if (early){
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

    while ((opt = getopt(argnum, argv, "s:l:h")) != -1){
        switch(opt){
            case 'h':
                printf("This program simulates an operating system with process scheduling.\n");
                printf("You can run it by ./oss\n");
                printf("You can also run it with -t [seconds], which will terminate the program after [seconds] seconds.\n");
                printf("Running the program this program with -h prints this message then terminates.\n");
                return -1;

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
