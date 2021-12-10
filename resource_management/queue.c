#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "queue.h"

Queue * initqueue(){
    Queue * queue;
    if ((queue = (Queue *) malloc(sizeof(Queue))) == NULL ) {
        fprintf(stderr, "Error in init queue");
        return NULL;
    }
    queue->front = queue->back = NULL;
    return queue;
}

void listQueue( Queue * queue ) {
    node * curr  = queue->front;
    while(curr != NULL) {
        fprintf(stderr, "%d ", curr->index);
        curr = curr->next;
    }
    fprintf(stderr, "\n");
}

void enqueue( Queue *queue, int pid) { 
	//Create a new node
        node * temp;
        if ((temp = ( node  *)malloc(sizeof(node))) == NULL) {
            perror("error making new queue node");
            return;
        }
        temp->index = pid;
        temp->next = NULL;

        //attatch the new node 
	if(queue_is_empty(queue)){
            queue->front = temp;
            queue->back = temp;
	    
	} else {
	    queue->back->next = temp;
	    queue->back = temp;
        }
}

int dequeue( Queue *queue ) {
    
    if(queue_is_empty(queue)) {
        return -1;
    }

    //Store previous front and move front one node ahead
    node * temp = queue->front;
    queue->front = queue->front->next;
	
    if (queue_is_empty(queue)){
	queue->back = NULL;
    }
    
    int result = temp->index; 
    free(temp);

    return result;
} 

bool queue_is_empty( Queue *queue ) {
    return queue->front == NULL;
}

void delete_queue( Queue * queue ) {
    while(!queue_is_empty(queue)) {
        dequeue(queue);
    }
}

