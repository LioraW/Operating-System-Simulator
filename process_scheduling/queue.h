#include <stdbool.h>

typedef struct item {
    int index;
    struct item *next;
} node;

typedef struct queue_t {
    node *front;
    node  *back;
} Queue;

Queue * initqueue ( void ); 
node  * newNode ( int index );
void enqueue ( Queue * queue, int index );
int dequeue( Queue * queue );
bool queue_is_empty( Queue * queue );
void delete_queue( Queue * queue );
void listQueue( Queue * queue );

