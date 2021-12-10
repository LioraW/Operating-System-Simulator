#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <stdbool.h>

#include "pcb_table.h"
#include "messenger.h"
#include "config.h"
#include "clock.h"
#include "queue.h"

#define BUF_SIZE 255
#define MAX 1024

#define msg1 1234
#define msg2 4321
#define maxTimeBetweenNewProcsNS 1
#define maxTimeBetweenNewProcsSecs 10
#define percentIOBoundProcs 60

typedef struct {
    int frame;          // which frame this page is in
    int permissions;    // if the process that has this page table is allowed to read, write, both or none on this page
    int validBit;       // if the frame is in memory or not
} PTE;


typedef struct {
    int page_num;
    int occupiedBit;
    unsigned int referenceBit : 8;
    int dirtyBit;
    unsigned int secPageInserted;
    unsigned int nanoPageInserted;
} frame_entry;

int validatearg(int argnum, char ** argv, char * filename, int * sec);
int print_error(char * msg){ fflush(stdout); perror(msg); return -1; }
int init_msg_queues(int * msgid1, int * msgid2);
int setup(int argc, char ** argv);
void setup_intr_handlers();
void sigint_handler(int);
void sigalarm_handler(int);
void termination(int early);
bool oneSecondHasPassed(unsigned int pastS);
int schedule();
int pageInFrame(int page);
int print_matrix(int ** matrix);
int child();
int page_replacement(int pagenum, int readwrite, int sim_pid);
void printFrameTable();
int sweepFrameTable();
double  percentFreeFrames();
int report();
int logmsg( char * msg, const char * filename);
bool timeForNewFork(unsigned int s, unsigned int n); 
