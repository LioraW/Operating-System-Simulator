#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include "messenger.h"
#define PERM (S_IRUSR | S_IWUSR )
#define MAX 1024


typedef struct {
    long mtype;
    contents content;
} message;

int init_msg_queue(const int key){
   int msgid;
   if ((msgid = msgget(key, 0666 | IPC_CREAT)) == -1) {
        perror("Error in init_msg_queue");
        return -1;
    }
    return msgid;
}

contents get_message(int msgid, int sim_pid, int msgflg){

   message msg;
    contents content = { -1, -1, -1, -1, -1};
    //get message
    if (msgrcv(msgid, &msg, sizeof(message), sim_pid + 1, msgflg) == ENOMSG) {
        return content;//means no mesage
    } else {
        content = msg.content;
    }

    return content;
}
int send_message(int msgid, int sim_pid, contents msgvalue){
    message msg;

    //Create message
    msg.mtype = sim_pid + 1; //can't have a type that is 0, so have to up each pid by one
    
    msg.content = msgvalue;

    //send message
    if (msgsnd(msgid, &msg, sizeof(message), 0 ) == -1) {
        //fprintf(stderr, "%ld:", msg.mtype);
        //fflush(stdout);
        //perror("failed to send message");
        return -1;
    }
    return 0;
}

int rem_msg_queue(int msgid) {
    return msgctl(msgid, IPC_RMID, NULL);
}




