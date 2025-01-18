#include "tests.h"
#include "queue.h"
#include <pthread.h>
#include <unistd.h>
#include "segel.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void runQueueTests() {
    struct timeval time1, time2, time3;
    gettimeofday(&time1, NULL);
    gettimeofday(&time2, NULL);
    gettimeofday(&time3, NULL);

    printf("=== Testing Queue Implementation ===\n");

    // Create queue with max size 3
    Queue q = queueCreate(3);
    queuePrint(q);

    // Enqueue requests
    printf("\n--- Enqueuing Requests ---\n");
    enqueue(q, 100, time1);
    queuePrint(q);
    enqueue(q, 200, time2);
    queuePrint(q);
    enqueue(q, 300, time3);
    queuePrint(q);

    // Check queue full
    printf("Queue full? %s\n", queueFull(q) ? "Yes" : "No");

    // Try to enqueue when full
    struct timeval time4;
    gettimeofday(&time4, NULL);
    enqueue(q, 400, time4);  // Should not be added
    queuePrint(q);

    // Find requests
    printf("\n--- Finding Requests ---\n");
    printf("Finding request with descriptor 200: Index %d\n", queueFindReq(q, 200));
    printf("Finding request with descriptor 999 (nonexistent): Index %d\n", queueFindReq(q, 999));

    // Dequeue request
    printf("\n--- Dequeue Requests ---\n");
    printf("Dequeued request descriptor: %d\n", dequeue(q));
    queuePrint(q);

    // Dequeue by index
    printf("\n--- Dequeue by Index ---\n");
    printf("Dequeued request at index 1: %d\n", dequeueByIndex(q, 1));  // Removes request 300
    queuePrint(q);

    // Check empty queue behavior
    dequeue(q); // Remove last request
    printf("\n");
    queuePrint(q);
    printf("Queue empty? %s\n", queueEmpty(q) ? "Yes" : "No");

    // Destroy queue
    printf("\nDestroying queue...\n");
    queueDestroy(q);
}


int main(){
    runQueueTests();
    return 0;
}
