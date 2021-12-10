#define NANOSECONDS_IN_SECOND 1000000000

int updateclock(unsigned int sec, unsigned int ns); // updates time. 
int readclock(unsigned int * seconds, unsigned int * nanoseconds); // reads shared memory into arguments
int initclock(); //preforms any needed initialization of the clock object
int cleanupclock(); //detach and remove shared memory
