
typedef struct message_contents {
    int timeused;
    int address_request;
    int readwrite;
    int frame_num;
    int terminated;
    int can_start;
} contents;

int rem_msg_queue(int msgid);
int init_msg_queue(const int key);
contents get_message(int msgid, int sim_pid, int msgflg); //sim_pid would be -1 if its oss using it
int send_message(int msgid, int msg_type, contents msgvalue);
