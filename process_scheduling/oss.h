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

//#include "pcb.h"
#include "pcb_table.h"
#include "messenger.h"
#include "config.h"
#include "clock.h"
#include "queue.h"

#define BUF_SIZE 100
#define MAX 1024

#define msg1 1234
#define msg2 4321
#define maxTimeBetweenNewProcsNS 1
#define maxTimeBetweenNewProcsSecs 10
#define percentIOBoundProcs 60

typedef struct for_averages {
   double cpu;
   double sys;
   double wait;
} total;

int validatearg(int argnum, char ** argv, char * filename, int * sec);
int print_error(char * msg){ fflush(stdout); perror(msg); return -1; }
int init_msg_queues(int * msgid1, int * msgid2);
int setup(int argc, char ** argv);
void setup_intr_handlers();
void sigint_handler(int);
void sigalarm_handler(int);
void termination(int early);
int schedule(Queue * queue, int currentQueue);
int child();
void setUnblockEventtime(int currentSec, int currentNS, int * futureSec, int * futureNS);
int logmsg( char * msg, const char * filename);
bool timeForNewFork(unsigned int s, unsigned int n); 
void logTotals(int pid, int waitTime);
