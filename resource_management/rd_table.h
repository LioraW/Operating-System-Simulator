#include "config.h"
#include <stdbool.h>
#include <semaphore.h>

//this structure represents a column in each of the matricies needed for the deadlock detection algorithm
typedef struct data_struct
{
        int ID;
	int request[NUM_PROCESSES][NUM_RESOURCES]; //array to keep track of who need how much of this process 
        int allocation[NUM_PROCESSES][NUM_RESOURCES]; //array to keep track of which processes have what
        int available[NUM_RESOURCES]; //avalible instances for this rd
        int maximum[NUM_PROCESSES][NUM_RESOURCES]; //maximum claims for each processes, for each resource
        int type[NUM_RESOURCES]; //shareable or not

} resource_descriptor;

enum rd_elem{ID, REQ, ALLOC, AVAIL, MAX, TYPE};
enum types{UNSHAREABLE, SHAREABLE};

//Getters
int getrequest(int rd, int pid);
int getallocation(int rd, int pid);
int getavailable(int rd);
int getmax(int rd, int pid);
int gettype(int rd);
//Setters
int setrequest(int entry, int pid, int request);
int setallocation(int entry, int pid,  int allocation);
int setavailable(int entry, int total);
int setmax(int entry, int pid, int max);
int setID(int id);

//public functions. modifying rd is protected by semaphores.
bool deadlock_avoidance(int pid, int * num_deadlocked);
int display_matrix(int matrix_symbol);
int modify_rd(int rd_num, int pid, int elem, int newval);
int getelement(int rd_num, int pid, int elem);
int init_rd(int rd_num, int num_resources, int rand_seed);
int  init_rd_table(void);
int get_rd_table(sem_t * semid, resource_descriptor * shm_table, int shmid);
int cleanup_rd(void);
