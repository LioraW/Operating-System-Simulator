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

#include "rd_table.h"
#include "messenger.h"
#include "config.h"
#include "clock.h"
#include "queue.h"

#define BUF_SIZE 255

#define msg1 1234
#define msg2 4321
#define maxTimeBetweenNewProcsNS 1
#define maxTimeBetweenNewProcsSecs 1
#define BOUND 250 //for userprocess bound

//setup functions
int setup(int argc, char ** argv);
int validatearg(int argnum, char ** argv, char * filename, int * sec);
int init_msg_queues(int * msgid1, int * msgid2);
void setup_intr_handlers();
void sigint_handler(int);
void sigalarm_handler(int);

//termination functions
void termination(int early);
int tell_process_to_terminate(int sim_pid);
int userprocess_termination(int sim_pid);

//main oss functions - run userprocesses, check processes, manage releases and requests
int schedule(Queue * queue);
int process_result(int sim_pid, contents result);
int process_release(int sim_pid, contents result);
int process_request(int sim_pid, contents result);
int child();

//timing functions
bool timeForNewFork(unsigned int s, unsigned int n);
void setUnblockEventtime(int currentSec, int currentNS, int * futureSec, int * futureNS);

//logging functions
void logstats();
int print_error(char * msg){ fflush(stdout); perror(msg); return -1; }
void log_available_resources();
int log_matrix(int matrix_symbol);
int logmsg( char * msg, const char * filename);

