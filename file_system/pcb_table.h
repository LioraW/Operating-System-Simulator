#include "config.h"

//pcb structure
typedef struct data_struct
{
        int        sim_pid;
        double     cpu_time_used;
        double     total_system_time;
        double     last_burst_time;
        int        process_priority;
        double     time_last_run;

} process_control_block;

enum pcb_elem{SIM_PID, CPU, SYS, BURST, PP, LAST_TIME};

//Getters
double getcputimeused(process_control_block pcb);
double getsystimeused(process_control_block pcb);
double getbursttime(process_control_block pcb);
int getprocesspriority(process_control_block  pcb);
double gettimeoflastrun(process_control_block pcb);

//Setters
int setcputimeused(process_control_block * entry, double cpu_time);
int setsystimeused(process_control_block * entry, double sys_time);
int setbursttime(process_control_block * entry, double burst_time);
int setsimpid(process_control_block * entry, int sim_pid);
int setpp(process_control_block * entry, int pp);
int settimeoflastrun(process_control_block * entry, double last_time);

//public functions. modifying pcb is protected by semaphores.
int modifypcb(int pid, int elem, double newdval, int newival);
double getelement(int pid, int elem);
int initpcb(int pid, int pp);
int init_pcb_table(void);
int cleanup_pcb(void);
