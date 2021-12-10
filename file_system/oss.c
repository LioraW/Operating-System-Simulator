/*
Project 6 Memory Management
Liora Wachsstock
Due: December 6, 2021
*/
#include "oss.h"              // has all include files, function prototypes, and constants

// Main data structures
Queue * highQueue;                          // process scheduling queue
int msgid_u2o, msgid_o2u;                   // two message queues, one from userprocess to oss, and vice versa
int pid_used[NUM_PROCESSES];                // for holding  which pids are being used
char exec_name[100];                        // executable name
frame_entry frame_table[NUM_FRAMES];        // Main Frame Table. 1d array, one for each frame, with info
PTE  page_tables[NUM_PROCESSES][NUM_PAGES]; // Main Page Table. 2D array, rows  = processes, columns = frame the page is in and process permissions for that page
frame_entry a[NUM_FRAMES];                  // for sorting Frame Table by age
frame_entry b[NUM_FRAMES];                  // for sorting Frame Table by age
pid_t childpid;                             // pid for child process

//for holding arguments
char * filename;
int sec;
int num_processes;

//Accounting
int numLines = 0;
int num_forked = 0;
int num_mem_refs = 0;
int num_page_faults = 0;
unsigned int access_time = 0; //for each process accesss time
unsigned int printTimeS = 0;

Queue * highQueue;

void d(int num) { fprintf(stderr, "\ndebug %d\n", num); }

int main(int argc, char ** argv){ 
    strcpy(exec_name, argv[0]); // get exec name
    pid_t wpid;                 // for waiting for children
    unsigned int s, n;          // for holding time
    int sim_pid = -1;           // to hold current simulated pid

    //set up. Modifies most of the global variables above.
    if (setup(argc, argv) != 0) {
        termination(0);
        return -1;
    }

    //initialize frame table
    for (int i = 0; i < NUM_FRAMES; i++){
        frame_table[i].page_num = -1; 
        frame_table[i].referenceBit = 0;
        frame_table[i].occupiedBit = 0;
        frame_table[i].dirtyBit = 0;
        frame_table[i].secPageInserted = 0;
        frame_table[i].nanoPageInserted = 0;
    }
    
    //initialize page table
    for (int i = 0; i < num_processes; i++){
        for (int j = 0; j < NUM_PAGES; j++){
            page_tables[i][j].frame = -1;
            page_tables[i][j].permissions = 3;
            page_tables[i][j].validBit = 0;
        }
    } //initialy none of the pages are in a frame
    

    //initialize bitmap that none of the pids are used (ie -1)
    for (int i = 0; i < NUM_PROCESSES; i++) {
        pid_used[i] = 0;
    }
    
    int open = 0;
    
    // read start time
    readclock(&s, &n);
    unsigned int lastGenTimeS = s;
    unsigned int lastGenTimeN = n;
    printTimeS = s;

    fprintf(stderr, "Running... (to show logs as it runs, uncomment the first line of  logmsg() in oss.c)\n");
    
    //loop until MAX_PROCESSES have been forked
    while(1){
        //in the begining there are no processes forked, so increment the clock until it is time to fork, or break if we forked enough times
        if (num_forked <= 0){
            updateclock(1, 0);
        } else if (num_forked >= MAX_PROCESSES) {
            break;
        }

        //generate a process every one second on the logical clock
        if (timeForNewFork(lastGenTimeS, lastGenTimeN)){
            for (int i = 0; i < num_processes; i++ ) {
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
        if ( open ) {
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
        schedule(highQueue);
        open = 0;
    
        //wait for children...
        while((( wpid = waitpid(-1, NULL, WNOHANG)) > 0) && (errno != EINTR));

        updateclock(1, rand() % 1000); //overhead for scheduling processes
   }
    
    //Let the rest of the processes finish up until they all terminate
    while(!queue_is_empty(highQueue)){
        schedule(highQueue);
        while((( wpid = waitpid(-1, NULL, WNOHANG)) > 0) && (errno != EINTR));
    }
    
    fprintf(stderr, " ******** DONE  ********* \n");

    //wait for  remainig children...
    while((( wpid = waitpid(-1, NULL, WNOHANG)) > 0) && (errno != EINTR)){
        sleep(2);
    }

    termination(0);

    return 0;
}    

int schedule(Queue * queue){
    unsigned int s,n;
    int sim_pid;
    char buf[BUF_SIZE];
    
    access_time = 0;
    
    contents content = { 0,0,0,0,0,0};
    content.can_start = 1; //signal that the process can start
    contents result;
    
    sim_pid = dequeue(queue);
    
    if ( sim_pid != -1 ){
        //Print table every one second on logical clock
        if (oneSecondHasPassed(printTimeS)){
                printFrameTable();
                readclock(&s, &n);
                printTimeS = s;
         }

        send_message(msgid_o2u, sim_pid, content); //send message to userprocess telling them we are ready
        result = get_message(msgid_u2o, sim_pid, 0);  //wait for process to send back the result
        
        readclock(&s, &n);
        //log
        buf[0] = '\0';
        snprintf(buf, sizeof(buf), "MASTER: P%d requesting %s of address %d  at time %d:%d\n", sim_pid, (result.readwrite == 0)? "read" : "write", result.address_request, s, n);
        logmsg(buf, filename);

        int framenum = -1;
        
        //check for invalid page request
        if (result.address_request >= NUM_PAGES*PAGE_SIZE) {
            //log
            buf[0] = '\0';
            snprintf(buf, sizeof(buf), "MASTER: P%d requested an invalid address, terminating process at time %d:%d\n", sim_pid,  s, n);
            logmsg(buf, filename);

            //send message to userprocess
            content.terminated = 1;
            send_message(msgid_o2u, sim_pid, content);
            
            pid_used[sim_pid] = 0;
            return 0;
        }
        
        //get page number from address
        int pagenum = result.address_request / PAGE_SIZE;
        num_mem_refs++;
        if ((framenum = pageInFrame(pagenum)) != -1 ) { //if page is in a frame

            //log
            buf[0] = '\0';
            snprintf(buf, sizeof(buf), "MASTER: Address %d  is in frame %d, %s P%d at time %d:%d\n", 
                                        result.address_request, framenum,(result.readwrite == 0)? "giving data to" : "writing data to frame for", sim_pid, s, n);
            logmsg(buf, filename);

            updateclock(0, 10);
            access_time += 10;

            //write to the dirty bit if we are writing to the frame
            if (result.readwrite == 1){
                frame_table[framenum].dirtyBit = 1;
            }
            
        } else { // PAGE FAULT

            //the process is suspended because it is still waiting for a message from OSS
            num_page_faults++;
            //log
            buf[0] = '\0';
            snprintf(buf, sizeof(buf), "MASTER: Address %d  is not in frame, page fault at time %d:%d\n", result.address_request, s, n);
            logmsg(buf, filename); 

            //deal with the page fault...
            framenum = page_replacement(pagenum, result.readwrite, sim_pid);
            
            //update system time
            updateclock(0, 14000000);
            access_time += 14000000;
            readclock(&s, &n);

            // log
            buf[0] = '\0';
            snprintf(buf, sizeof(buf), "MASTER: Cleared frame %d and swapping in P%d page %d at time %d:%d\n", framenum, sim_pid, pagenum, s, n);
            logmsg(buf, filename);

        }
        
        //send message to userprocess
        content.frame_num = framenum;
        content.timeused = access_time;
        send_message(msgid_o2u, sim_pid, content);
        
        //set the most significant bit of the reference byte because this frame was referenced.
        frame_table[framenum].referenceBit |= 1 << 7; //since there are 8 bits, the most significant bit is in the 7th place.
        
        num_mem_refs++; //for shifting the reference bit
        if (num_mem_refs % SHIFT_INTERVAL == 0){
            for (int i = 0; i < NUM_FRAMES; i++)
                frame_table[i].referenceBit >>= 1;
        }

        //sweep table if less than 10% of the total frames are free
        if ( percentFreeFrames() < 10){
             //log
            buf[0] = '\0';
            snprintf(buf, sizeof(buf), "MASTER: Frame table less than 10 percent free, running daemon\n");
            logmsg(buf, filename);

            sweepFrameTable();
        }
 
        if (result.terminated == 1){
            //log
            buf[0] = '\0';
            snprintf(buf, sizeof(buf), "MASTER: Receiving that process with PID %d also terminated with total memory access time %d nanoseconds\n", sim_pid, result.timeused);
            logmsg(buf, filename);

            pid_used[sim_pid] = 0; //done with this pid
        
        } else { //if it did not terminate, run it again
             enqueue(highQueue, sim_pid);
        }

    }
    access_time = 0; //reset global

    return 0;
}

double percentFreeFrames(){
    int numfree = 0;
    for (int i = 0; i < NUM_FRAMES; i++)
        if (frame_table[i].occupiedBit == 0)
            numfree++;

    return ( (double) numfree / (double) NUM_FRAMES) * 100.0;
}


int insertPage(int pagenum, int framenum, int readwrite){
    unsigned int s, n;

    int oldPageNum = -1;
    if ((oldPageNum = frame_table[framenum].page_num) != -1){
        //tell page tables that the page to be replaced is no longer in memory
        for (int i = 0; i < num_processes; i++){
            page_tables[i][oldPageNum].frame = -1;
            page_tables[i][oldPageNum].validBit = 0;
        }  
    }
    
    //replace the page
    readclock(&s, &n);
    frame_table[framenum].page_num = pagenum;
    frame_table[framenum].occupiedBit = 1;
    frame_table[framenum].dirtyBit = (readwrite == 1) ? 1 : 0;
    frame_table[framenum].referenceBit = 0;
    frame_table[framenum].secPageInserted = s;
    frame_table[framenum].nanoPageInserted = n;

    //tell the page tables where the page is
    for (int i = 0; i < num_processes; i++){
        page_tables[i][pagenum].validBit = 1;
        page_tables[i][pagenum].frame = framenum;
    }
    return framenum;
}

int page_replacement(int pagenum, int readwrite, int sim_pid) {
    int framenum = -1;
    char buf[BUF_SIZE];
    
    //Look for an open frame, if there is one put it there
    int i = 0;
    for ( ;i < NUM_FRAMES; i++){
        if (frame_table[i].occupiedBit != 1){
            framenum = insertPage(pagenum, i, readwrite);
            break;
        }    
    }
    //if we went through the entire frame table and there were no open frames, find the frame with the smallest reference byte
    if (i == NUM_FRAMES){
        
        // ADDITIONAL REFERENCE BITS ALGORITHM
        int smallestByte = frame_table[0].referenceBit; //start with the first one
        int oldest_index = 0;                           //hold index of LRU frame
        for (int i = 1; i < NUM_FRAMES; i++){
            if (frame_table[i].referenceBit < smallestByte){
                smallestByte = frame_table[i].referenceBit;
                oldest_index = i;
            }
        }
        
        //check the status of the page we are going to remove
        if (frame_table[oldest_index].dirtyBit){
            updateclock(0, 15000000); //take a long time to write the data back to the disk 
            access_time += 15000000;
            //log
            buf[0] = '\0';
            snprintf(buf, sizeof(buf), "MASTER: Dirty bit of frame %d set, adding additional time to the clock\n", oldest_index);
            logmsg(buf, filename);
        }

        //replace the page
        framenum = insertPage(pagenum, oldest_index, readwrite);
         
        //set the page_table for that process
        page_tables[sim_pid][pagenum].frame = oldest_index;

    } else {
        //set the page_table for that process
        page_tables[sim_pid][pagenum].frame = i;
    }
    
    return framenum;
}

int pageInFrame(int page){
    for (int i = 0; i < NUM_FRAMES; i++){
        if (frame_table[i].page_num == page){
            return i;
        }
    }
    return -1;
}

/* for Table Sweeping daemon */

//Merge sort code from https://www.tutorialspoint.com/data_structures_algorithms/quick_sort_program_in_c.htm
void merging(int low, int mid, int high) {
   int l1, l2, i;

   for(l1 = low, l2 = mid + 1, i = low; l1 <= mid && l2 <= high; i++) {
      if( atLeastAsOld(a[l1].secPageInserted, a[l1].nanoPageInserted, a[l2].secPageInserted, a[l2].nanoPageInserted))
         b[i] = a[l1++];
      else
         b[i] = a[l2++];
   }
   
   while(l1 <= mid)    
      b[i++] = a[l1++];

   while(l2 <= high)   
      b[i++] = a[l2++];

   for(i = low; i <= high; i++)
      a[i] = b[i];
}

void sort_frames_by_age(int low, int high) {
   int mid;
   
   if(low < high) {
      mid = (low + high) / 2;
      sort_frames_by_age(low, mid);
      sort_frames_by_age(mid+1, high);
      merging(low, mid, high);
   } else { 
      return;
   }   
}

int sweepFrameTable(){
    
    int n = NUM_PAGES *(5/100); //number to clear
    int num_cleared = 0;
    
    //copy current frame table and sort it by age
    for (int i = 0; i < NUM_FRAMES; i++){
        a[i] = frame_table[i];
    }
    sort_frames_by_age(NUM_FRAMES, 0);
    
    //Get the youngest age of all of the pages in n oldest pages
    unsigned int youngestOfOldest_s = a[ NUM_FRAMES - n ].secPageInserted;
    unsigned int youngestOfOldest_n = a[ NUM_FRAMES - n ].nanoPageInserted;

   //go through the frame table
    for (int i = 0; i < NUM_FRAMES; i++){
        
        //only clear n frames
        if ( num_cleared > n ){
            break;
        } else {
            //check if the current frame is older (or same age as) the youngest frame of the oldest n frames
            if ( atLeastAsOld(frame_table[i].secPageInserted, frame_table[i].nanoPageInserted, youngestOfOldest_s, youngestOfOldest_n) ){
                bool validBitOff = false;
                //turn valid bit off
                for (int j = 0; j < num_processes; j++){
                    
                    //if the valid bit is already turned off, clear the frame completely
                    if (page_tables[j][frame_table[i].page_num].validBit ==  0){
                        validBitOff = true;
                        page_tables[j][frame_table[i].page_num].frame = -1;
                        page_tables[j][frame_table[i].page_num].validBit = 0;
                        
                    } else {
                        page_tables[j][frame_table[i].page_num].validBit = 0;
                    }
                    
                }
                
                if (validBitOff){
                    if (frame_table[i].dirtyBit == 1){
                        updateclock(0, 14000000); //take a while for writing back data
                    }
                    //clear frame completely
                    frame_table[i].page_num = -1;
                    frame_table[i].occupiedBit = 0;
                    frame_table[i].dirtyBit = 0;
                    frame_table[i].referenceBit = 0;
                    frame_table[i].secPageInserted = 0;
                    frame_table[i].nanoPageInserted = 0;

                }
                num_cleared++;
            }
        }
    }
    
    return 0;
}

/* Useful functions */
bool oneSecondHasPassed(unsigned int pastS){
        unsigned int s, n;
        readclock(&s, &n);
        return s > pastS;
}

bool timeForNewFork(unsigned int lastGenTimeS, unsigned int lastGenTimeN) {
    unsigned int s, n;
    readclock(&s, &n);
    return  (s > (lastGenTimeS + (rand() % maxTimeBetweenNewProcsSecs))) 
        && (n > (lastGenTimeN + (rand() % maxTimeBetweenNewProcsNS)));
}

int child(int sim_pid) {
    char buf1[BUF_SIZE];

    snprintf(buf1, sizeof(buf1), "%d", sim_pid);
    //snprintf(buf2, sizeof(buf2), "%d", cpuBoundOrIOBound() ); 
    
    execl("./userprocess", "userprocess", buf1, NULL);
    perror("execl");

    return -1;
}

/* Logging Functions */
void printFrameTable(){
        char buf[BUF_SIZE];
        unsigned int s, n;

        readclock(&s, &n);
        buf[0] = '\0';
        snprintf(buf, sizeof(buf), "\nCurrent Memory Layout at time %d:%d is:\n", s, n);
        logmsg(buf, filename);

        buf[0] = '\0';
        snprintf(buf, sizeof(buf), "      Occupied | Refbyte | DirtyBit | Page in Frame\n");
        logmsg(buf, filename);

    for (int i = 0; i < NUM_FRAMES; i++){
                buf[0] = '\0';
        snprintf(buf, sizeof(buf),"Frame %d: %s%d       %d         %d        %d\n",
                i, (i > 9) ? " " : "  ", frame_table[i].occupiedBit, frame_table[i].referenceBit, frame_table[i].dirtyBit, frame_table[i].page_num);
                logmsg(buf, filename);
    }

}

int report(){
    char buf[BUF_SIZE];
    unsigned int s, n;

    readclock(&s, &n);
    buf[0] = '\0';
    snprintf(buf, sizeof(buf), "\n\n ****************  REPORT ****************** \n");
    logmsg(buf, filename);


    buf[0] = '\0';
    snprintf(buf, sizeof(buf), "- Number of memory accesses per second:  %lf\n", ((s != 0) ? ((double)num_mem_refs/(double)s) : 0 )
                                                                               + ((n != 0) ? ((double)num_mem_refs/ (double)n) : 0)
                                                                               *             (1/(double)NANOSECONDS_IN_SECOND ));
    logmsg(buf, filename);

    buf[0] = '\0';
    snprintf(buf, sizeof(buf), "- Number of page faults per memory access: %lf\n", (num_mem_refs != 0)? (double)num_page_faults/(double)num_mem_refs : 0 );
    logmsg(buf, filename);

    buf[0] = '\0';
    snprintf(buf, sizeof(buf), "- Average memory access speed: %lf\n accesses per second", ((s != 0) ? ((double)num_mem_refs/(double)s) : 0 )
                                                                               + ((n != 0) ? ((double)num_mem_refs/ (double)n) : 0)
                                                                               *             (1/(double)NANOSECONDS_IN_SECOND ));
    logmsg(buf, filename);
    
    return 0;

}
// Log messages in logfile
int logmsg(char * msg, const char * fname) {
    fprintf(stderr, "\n%s", msg);
    
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

/* setup and termination functions */

int setup ( int argc, char ** argv) {

    if ((filename = malloc(BUF_SIZE*sizeof(char))) ==  NULL) {
        perror("Malloc in main oss");
        return -1;
    }
    
    if (validatearg(argc, argv, filename, &sec) != 0){
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
        report();
        delete_queue(highQueue);
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
    num_processes = NUM_PROCESSES;
    int number, opt;

    //in case the user does not supply a file name, use "logfile"
    strcpy(filename, "logfile");

    while ((opt = getopt(argnum, argv, "s:l:p:h")) != -1){
        switch(opt){
            case 'h':
                printf("This program simulates an operating system with memory management.\n");
                printf("You can run it by ./oss\n");
                printf("You can also run it with -t [seconds], which will terminate the program after [seconds] seconds.\n");
                printf("You can run it with -p [number] will will create a max of [number] userprocess. The argument cannot be larger than the NUM_PROCESSES constant in config.h\n");
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
            case 'p':
                //check if arg is a number
                number = strtol(optarg, &optarg, 10);
                if (*optarg != '\0') {
                    errno=1;
                    print_error("The argument supplied is not a number");
                    return -1;
                }else {
                    //take the number
                    if (number > NUM_PROCESSES){
                        print_error("The argument supplied is too large");
                        return -1;
                    } else {
                        num_processes = number;
                    }
                }

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
