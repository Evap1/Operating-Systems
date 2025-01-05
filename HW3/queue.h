#include <stdbool.h>
#include <sys/time.h>

typedef struct Queue *Queue;
typedef struct Node *Node;

Node node_create(int value, struct timeval arrival);

Queue queue_create(int size);

int queue_size(Queue q);

bool queue_full(Queue q);

bool queue_empty(Queue q);

void enqueue(Queue q, int value, struct timeval arrival);

struct timeval queue_head_arrival_time(Queue q);

int dequeue(Queue q);

int queue_find(Queue q, int value);

int dequeue_index(Queue q, int index);

void queue_destroy(Queue q);

void queue_print(Queue q);