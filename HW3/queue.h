#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>
#include <sys/time.h>

typedef struct Queue *Queue;
typedef struct Request *Request;

Request requestCreate(int descriptor, struct timeval arrival);

Queue queueCreate(int size);

int queueSize(Queue q);

bool queueFull(Queue q);

bool queueEmpty(Queue q);

void enqueue(Queue q, int value, struct timeval arrival);

struct timeval queue_headArrivalTime(Queue q);

int dequeue(Queue q);

int queueFindReq(Queue q, int value);

int dequeueByIndex(Queue q, int index);

void queueDestroy(Queue q);

void queuePrint(Queue q);

#endif // QUEUE_H