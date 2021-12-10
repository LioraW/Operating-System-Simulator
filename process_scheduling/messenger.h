typedef struct message_contents {
    int timeused;
    int blocked;
    int terminated;
    int can_start;
    int usedIO;
} contents;

enum { can_start = -4, ready = -3, blocking = -2, error = -1 };

int rem_msg_queue(int msgid);
int init_msg_queue(const int key);
contents get_message(int msgid, int sim_pid, int msgflg); //sim_pid would be -1 if its oss using it
int send_message(int msgid, int msg_type, contents msgvalue);
