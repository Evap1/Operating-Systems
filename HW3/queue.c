#include <stdio.h>
#include "queue.h"

#include <stdlib.h>

struct Node {
    int data;
    struct timeval arrival;
    Node next;
};

struct Queue {
    int max_size;
    int current_size;
    Node head;
    Node tail;
};

Node node_create(int value, struct timeval arrival){
    Node node = (Node)malloc(sizeof(Node*));
    node->data = value;
    node->arrival = arrival;
    node->next = NULL;
    return node;
}

Queue queue_create(int size){
    Queue q = (Queue)malloc(sizeof(Queue*));
    q->head = NULL;
    q->tail = NULL;
    q->current_size = 0;
    q->max_size = size;
    return q;
}

bool queue_full(Queue queue){
    if(queue->current_size == queue->max_size)
        return true;
    else
        return false;
}

bool queue_empty(Queue queue){
    return queue->current_size == 0;
}

void enqueue(Queue q, int value, struct timeval arrival){
    if(queue_full(q))
        return;

    Node new_node = node_create(value, arrival);
    if(queue_empty(q)){
        q->head = new_node;
        q->tail = new_node;
    } else {
        q->tail->next = new_node;
        q->tail = new_node;
    }
    q->current_size++;
}

struct timeval queue_head_arrival_time(Queue q){
    if(queue_empty(q))
        return (struct timeval){0};
    return q->head->arrival;
}

int dequeue(Queue q){
    if(queue_empty(q))
        return -1;
    Node temp = q->head->next;
    int value = q->head->data;
    free(q->head);
    if(temp == NULL){
        q->head = NULL;
        q->tail = NULL;
    } else {
        q->head = temp;
    }
    q->current_size--;
    return value;
}

int queue_find(Queue q, int value){
    if(queue_empty(q))
        return -1;
    Node temp = q->head;
    int index = 0;
    while(temp){
        if(value == temp->data){
            return index;
        }
        index++;
        temp = temp->next;
    }

    return -1;
}

int dequeue_index(Queue q, int index){
    if(queue_empty(q))
        return -1;

    if(index < 0 || index >= queue_size(q))
        return -1;

    if(index == 0){
        return dequeue(q);
    }

    Node node_to_dequeue = q->head;
    Node prev_node = NULL;
    for(int i = 0; i < index; i++){
        prev_node = node_to_dequeue;
        node_to_dequeue = node_to_dequeue->next;
    }

    int value = node_to_dequeue->data;
    prev_node->next = node_to_dequeue->next;
    free(node_to_dequeue);
    if(index == queue_size(q) - 1){
        q->tail = prev_node;
    }
    q->current_size--;

    return value;
}

int queue_size(Queue q){
    return q->current_size;
}

void queue_destroy(Queue q){
    Node current = q->head;
    Node next = NULL;
    while (current){
        next = current->next;
        free(current);
        current = next;
    }

    free(q);
}

void debug(Queue q){
    if (queue_empty(q)){
        printf("Queue is empty\n");
        return;
    }

    Node temp = q->head;
    while (temp){
        printf("%d ", temp->data);
        temp = temp->next;
    }
    printf("\n");
}

