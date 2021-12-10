#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include "clock.h"

#define SHMKEY_SECONDS 613613
#define SHMKEY_NANOSECONDS 123123
#define BUFF_SZ sizeof(unsigned int)
#define PERMS (S_IRUSR | S_IWUSR)

// Globals
int error;          // for holding error codes

//Shared memory
int shmid_seconds;
char * paddr_seconds;

int shmid_nanoseconds;
char * paddr_nanoseconds;

//Semaphores
int semid;
struct sembuf semsignal[1];
struct sembuf semwait[1];
void printerror(char * msg){
    fprintf(stderr, "[%ld]:", (long)getpid());
    fflush(stdout);
    perror(msg);
}

/* Semaphore operations inspired by Robbins pg 521 */
void setsembuf ( struct sembuf *s, int num, int op, int flg ) {
    s->sem_num = (short)num;
    s-> sem_op=(short)op;
    s-> sem_flg = (short)flg;
    
    return;
}

int initelement(int semid, int semnum, int semvalue ) {

    union semnum {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
        } arg;
    arg.val = semvalue;
    return semctl(semid, semnum, SETVAL, arg);

}

int removesem(int semid) {
    return semctl(semid, 0, IPC_RMID);
}

int r_semop( int semid, struct sembuf *sops, int nsops) {
    while ( semop(semid, sops, nsops) == -1 ){
        if (errno != EINTR){
            printerror("semop error");
            return -1;
        }
    }
    return 0;
}

int updateclock(unsigned int sec, unsigned ns){
    int keep_waiting = 1;
    unsigned int *seconds  = NULL;
    unsigned int *nanoseconds = NULL;

    do
    {
        /* Entry Section */
        if ( (error = r_semop(semid, semwait, 1)) == -1 ){
            printerror("Failed to lock semid");
            return -1;
        }else if ( !error ) {

            /* Critical Section */
            seconds = ( unsigned int * ) ( paddr_seconds );
            nanoseconds = ( unsigned int * ) ( paddr_nanoseconds );
            
            *nanoseconds += ns;
            *seconds += sec;

            //check if there was overflow in nanoseconds
            if (*nanoseconds > NANOSECONDS_IN_SECOND){
                *nanoseconds -= NANOSECONDS_IN_SECOND;
                *seconds += 1;
        }
            
        keep_waiting = 0;

        /* Signal */
        if ((error = r_semop(semid, semsignal, 1)) == -1) {
            printerror("Failed to unlock semid");
            return -1;
        }
    }
        /* if it did not break, continue waiting */
        if ( keep_waiting ){
            sleep(1);
        }
   } while ( keep_waiting );
    return 0;
}

int readclock(unsigned int * read_seconds, unsigned int * read_nanoseconds){
    
    /* Wait */
    if ((error = r_semop(semid, semwait, 1)) == -1) {
        printerror("Failed to lock semid");
        return -1;
    }else if (!error){

        /* Critical Section */
            unsigned int * seconds = ( unsigned int * ) ( paddr_seconds );
            unsigned int * nanoseconds = ( unsigned int * ) ( paddr_nanoseconds );
            *read_seconds = *seconds;
            *read_nanoseconds = *nanoseconds;

        /* Signal */
        if ((error = r_semop(semid, semsignal, 1)) == -1){
            printerror("Failed to unlock semid");
            return -1;
        }
    }
    return 0;
}
bool atLeastAsOld(unsigned int s1, unsigned int n1, unsigned int s2, unsigned int n2){

    if (s1 > s2){
        return true;
    } else if ( s1 == s2 ){
        if ( n1 > n2){
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

int initclock() {

    // Create and initialize  semaphore containing single element
    if ((semid = semget(IPC_PRIVATE, 1, PERMS)) == -1){
        printerror("Failed to create private semaphore");
        return -1;
    }
    setsembuf(semwait, 0, -1, 0); /* Decrement element 0 */
    setsembuf(semsignal, 0, 1, 0);/* Increment element 0 */
    
    if ( initelement(semid, 0, 1) == -1) {
        printerror("Failed to initilize semaphore element to 1");
        if (removesem(semid) == -1){
            printerror("Failed to remove failed semaphore");
        }
        return -1;
    }

    // Initialize clock
    shmid_seconds = shmget ( SHMKEY_SECONDS, BUFF_SZ, 0777 | IPC_CREAT );
    if ( shmid_seconds == -1 ){
        printerror("Error in shmget initclock - seconds");
        return -1;
    }
    shmid_nanoseconds = shmget( SHMKEY_NANOSECONDS, BUFF_SZ, 0777 | IPC_CREAT );
    if ( shmid_nanoseconds == -1 ){
        printerror("error in shmget initclock - nanoseconds");
        return -1;
    }
    //attach the address
    paddr_seconds  = ( char  * ) (shmat( shmid_seconds, 0, 0));
    paddr_nanoseconds = ( char * )(shmat( shmid_nanoseconds, 0, 0));

    //initialize with a value
    unsigned int * seconds  = ( unsigned int * ) ( paddr_seconds );
    unsigned int * nanoseconds = ( unsigned int * )( paddr_nanoseconds );
    *seconds = 0;
    *nanoseconds = 0;

    return 0;
}

// Clean up shared memory and semaphores
int cleanupclock() {
    // shared mem
    shmdt(paddr_seconds);
    if (shmctl(shmid_seconds, IPC_RMID, NULL) == -1)
        perror("shmctl_seconds");
    // shared mem
    shmdt(paddr_nanoseconds);
    if (shmctl(shmid_nanoseconds, IPC_RMID, NULL) == -1)
        perror("shmctl_nanoseconds");

    //shared mem semaphore
    if ((error = removesem(semid)) == -1){
        printerror("Failed to clean up semaphore");
    }

    return 0 ;
}

