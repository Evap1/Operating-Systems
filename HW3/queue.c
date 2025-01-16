#include "queue.h"
#include "segel.h"

struct Request {
    int descriptor;
    struct timeval arrival;
    Request next;
};

struct Queue {
    int max_size;
    int current_size;
    Request head;
    Request tail;
};

// Given a descriptor, create a new request
// helper function
Request requestCreate(int descriptor, struct timeval arrival){
    Request node = (Request)malloc(sizeof(*node));
    node->descriptor = descriptor;
    node->arrival = arrival;
    node->next = NULL;
    return node;
}

// Create empty queue of max_size = size
Queue queueCreate(int size){
    Queue q = (Queue)malloc(sizeof(Queue*));
    q->head = NULL;
    q->tail = NULL;
    q->current_size = 0;
    q->max_size = size;
    return q;
}

// return true if the currect queue is full
// helper function
int queueFull(Queue queue){
    if(queue->current_size == queue->max_size)
        return 1;
    else
        return 0;
}

// return true if the currect queue is empty
// helper function
int queueEmpty(Queue queue){
    return queue->current_size == 0;
}

// add new request to queue, if full does nothing
// function to use queue
void enqueue(Queue q, int descriptor, struct timeval arrival){
    if(queueFull(q))
        return;

    Request new_node = requestCreate(descriptor, arrival);
    if(queueEmpty(q)){
        q->head = new_node;
        q->tail = new_node;
    } else {
        q->tail->next = new_node;
        q->tail = new_node;
    }
    q->current_size++;
}

// pop head request and return it's descriptor, if empty, returns -1.
// function to use queue
int dequeue(Queue q){
    if(queueEmpty(q))
        return -1;
    Request new_head = q->head->next;
    int descriptor = q->head->descriptor;
    free(q->head);
    if(new_head == NULL){
        q->head = NULL;
        q->tail = NULL;
    } else {
        q->head = new_head;
    }
    q->current_size--;
    return descriptor;
}


// pop tail request and return it's descriptor, if empty, returns -1.
// used for policy dt
int dequeueTail(Queue q){
    if(queueEmpty(q))
        return -1;
    Request tail = q->tail;
    Request new_tail = q->head;
    int descriptor = q->tail->descriptor;

    if(new_tail == NULL){
        q->head = NULL;
        q->tail = NULL;
    } else {
        while(new_tail->next != tail) {
            new_tail = new_tail->next;
        }
        new_tail->next = NULL;
    }

    free(q->tail);
    q->current_size--;
    return descriptor;
}

// for policy - random
void randomDequeue(Queue q) {
    if(queueEmpty(q)) {
        return;
    }
    int num_req_to_be_deleted = q->current_size / 2;
    int deleted = 0;
    Request r = q->head;

    while(deleted < num_req_to_be_deleted) {
        if(r == q->tail->next) {                //if at the end of the list but still in loop, return to head of the queue
            r = q->head;                        //meaning we haven't killed enough in the list
        }

        int rand_index = rand() % 2;            //deciding if we want to kill - 1 for kill, 0 for spare
        if(rand_index) {                        //1 - kill
            int fd_to_be_deleted = q->head->descriptor;
            Close(fd_to_be_deleted);
            dequeueByReq(q, fd_to_be_deleted);
            deleted++;
        }
        r = r->next;
    }

}

// pop the latest request == the last in queue
// if empty, return -1.
int skipDequeue(Queue q){
    if (queueEmpty(q)){
        return -1;
    }

    int last_index = queueSize(q) - 1;
    return dequeueByIndex(q , last_index);
}

int headDesciprot(Queue q){
    if(queueEmpty(q))
        return -1;
    return q->head->descriptor;
}

// return time of arrival of head request (first added), if empty returns 0
struct timeval queueHeadArrivalTime(Queue q){
    if(queueEmpty(q))
        return (struct timeval){0};
    return q->head->arrival;
}

// return the index in queue of request, if not found or empty return -1.
int queueFindReq(Queue q, int descriptor){
    if(queueEmpty(q))
        return -1;
    Request temp = q->head;
    int index = 0;
    while(temp){
        if(descriptor == temp->descriptor){
            return index;
        }
        index++;
        temp = temp->next;
    }
    return -1;
}

// pop request with the given descriptor
void dequeueByReq(Queue q, int descriptor){
    int index = queueFindReq(q, descriptor);
    if (index == -1) {
        return;
    }
    dequeueByIndex(q, index);
}

// pop request in the given index, if empty or invalid index return -1, else return the descriptor of the request
int dequeueByIndex(Queue q, int index){
    if(queueEmpty(q))
        return -1;

    if(index < 0 || index >= queueSize(q))
        return -1;

    if(index == 0){
        return dequeue(q);
    }

    Request node_to_dequeue = q->head;
    Request prev_node = NULL;
    for(int i = 0; i < index; i++){
        prev_node = node_to_dequeue;
        node_to_dequeue = node_to_dequeue->next;
    }

    int value = node_to_dequeue->descriptor;
    prev_node->next = node_to_dequeue->next;
    free(node_to_dequeue);
    if(index == queueSize(q) - 1){
        q->tail = prev_node;
    }
    q->current_size--;

    return value;
}

int queueSize(Queue q){
    return q->current_size;
}

void queueDestroy(Queue q){
    Request current = q->head;
    Request next = NULL;
    while (current){
        next = current->next;
        free(current);
        current = next;
    }

    free(q);
}

void debug(Queue q){
    if (queueEmpty(q)){
        printf("Queue is empty\n");
        return;
    }

    Request temp = q->head;
    while (temp){
        printf("%d ", temp->descriptor);
        temp = temp->next;
    }
    printf("\n");
}

void queuePrint(Queue q){
    if (queueEmpty(q)){
        printf("Queue is empty\n");
        return;
    }

    printf("Queue (size: %d/%d): ", q->current_size, q->max_size);
    Request temp = q->head;
    while (temp){
        printf("[Descriptor: %d | Arrival: %ld.%06ld] -> ", temp->descriptor, temp->arrival.tv_sec, temp->arrival.tv_usec);
        temp = temp->next;
    }
    printf("NULL\n");
}