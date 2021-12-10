typedef struct message_contents {
    int resource_id;
    int requesting;
    int releasing;
    int timeused;
    int terminated;
} contents;

int rem_msg_queue(int msgid);
int init_msg_queue(const int key);
contents get_message(int msgid, int sim_pid, int msgflg); //sim_pid would be -1 if its oss using it
int send_message(int msgid, int msg_type, contents msgvalue);
