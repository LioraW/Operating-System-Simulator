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
    char mtext[100];
} message;

int init_msg_queue(const int key){
   int msgid;
   if ((msgid = msgget(key, 0666 | IPC_CREAT)) == -1) {
        perror("Error in init_msg_queue");
        return -1;
    }
    return msgid;
}

contents process_message(char * msgtext) {
    const char delim[2] = " ";
    char * token;
    contents content;
    token = strtok(msgtext, delim);

   // walk through other tokens, find 4th column which is the time
    int i = 0;
    while( token != NULL ) {
        switch(i) {
            case 0:
                content.timeused = atoi(token);
                break;
            case 1:
                content.blocked = atoi(token);
                break;
            case 2:
                content.terminated = atoi(token);
                break;
            case 3:
                content.can_start = atoi(token);
                break;
            case 4:
                content.usedIO = atoi(token);
                break;
        }

      token = strtok(NULL, delim);
      i = i + 1;
   }
   return content;
}

contents get_message(int msgid, int sim_pid, int msgflg){

   message msg;
    contents content = { -1, -1, -1, -1, -1};
    //get message
    if (msgrcv(msgid, &msg, sizeof(message), sim_pid + 1, msgflg) == ENOMSG) {
        return content;//means no mesage
    } else {
        content = process_message(msg.mtext);
    }

    return content;
}
int send_message(int msgid, int sim_pid, contents msgvalue){
    message msg;

    //Create message
    msg.mtype = sim_pid + 1; //can't have a type that is 0, so have to up each pid by one
    //create mesage text 
    char messagetext[100];
    snprintf(messagetext, sizeof(messagetext), "%d %d %d %d %d", msgvalue.timeused, msgvalue.blocked, msgvalue.terminated, msgvalue.can_start, msgvalue.usedIO);

    strcpy(msg.mtext, messagetext);
    
    //send message
    if (msgsnd(msgid, &msg, sizeof(message), 0 ) == -1) {
        fprintf(stderr, "%ld:", msg.mtype);
        fflush(stdout);
        perror("failed to send message");
        return -1;
    }
    return 0;
}

int rem_msg_queue(int msgid) {
    return msgctl(msgid, IPC_RMID, NULL);
}




