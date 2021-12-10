#include <stdbool.h>
#define NANOSECONDS_IN_SECOND 1000000000
bool atLeastAsOld(unsigned int s1, unsigned int n1, unsigned int s2, unsigned int n2);
int updateclock(unsigned int sec, unsigned int ns); // updates time. 
int readclock(unsigned int * seconds, unsigned int * nanoseconds); // reads shared memory into arguments
int initclock(); //preforms any needed initialization of the clock object
int cleanupclock(); //detach and remove shared memory
