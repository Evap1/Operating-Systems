#include "segel.h"
#include "request.h"
#include "queue.h"
#include "pthread.h"
#define BUFFER 10
#define NUM_OF_POLICIES 5

//
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//
int DEBUG = 0;

// DECLERATIONS
void initMaster(int threads_num, int queue_size, pthread_t *worker_threads, pthread_t *vip_thread);
void *workerThread(void *thread_number);
void *vipThread(void *thread_number);
void getargs(int *port, int *threads_num, int *queue_size, char *policy, int argc, char *argv[]);
int totalReqInQueue();
int isValidPolicy(char *policy);

// GLOBALS
Queue handeling_requests, waiting_requests, vip_waiting_requests;
pthread_mutex_t lock;
pthread_cond_t new_req_allowed, vip_allowed, worker_allowed, empty_queue, full_queue;
int queue_size;
int handeling_vip;
const char *common_policies[] = {"block", "dh", "dt", "bf", "random"};

int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen, threads_num, is_vip;
    char policy[BUFFER];
    struct sockaddr_in clientaddr;
    struct timeval arrival;

    getargs(&port, &threads_num, &queue_size, policy, argc, argv);
    pthread_t *worker_threads = malloc(sizeof(pthread_t) * threads_num);
    pthread_t vip_thread;
    initMaster(threads_num, queue_size, worker_threads, &vip_thread);

    // start listening
    listenfd = Open_listenfd(port);

    while (1)
    {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);

        pthread_mutex_lock(&lock); // -------------------------------->

        gettimeofday(&arrival, NULL);
        is_vip = getRequestMetaData(connfd); // check if regular or vip
        if (DEBUG)
        {
            printf("[main] : recieved a task at: %ld", arrival.tv_sec);
        }

        // proccess full queue according to policy
        if (totalReqInQueue() >= queue_size)
        {
            // if the waiting list is empty or block, then block as usual
            if (queueSize(waiting_requests) == 0 || strcmp(policy, "block") == 0)
            { // as it wassssssss
                while (totalReqInQueue() >= queue_size)
                { // waiting for place in wait queue
                    pthread_cond_wait(&new_req_allowed, &lock);
                }
            }
            else if (strcmp(policy, "dt") == 0)
            { // drop tail - brand new request recieved
                if (!is_vip)
                {                                // req is regular - current is considered tail
                    Close(connfd);               // after empty drop worker hoe req
                    pthread_mutex_unlock(&lock); // ------------------------------^
                    continue;
                }
                else
                {                                                     // req is vip - remove from regular
                    int fd_to_delete = dequeueTail(waiting_requests); // try to dequeue from waiting
                    if (fd_to_delete != -1)
                    { // should always succeed since the waiting should not be empty here
                        Close(fd_to_delete);
                    }
                }
            }
            else if (strcmp(policy, "dh") == 0)
            { // drop head - brand old request recieved
                int fd_to_delete = dequeue(waiting_requests);
                if (fd_to_delete != -1)
                {                        // check if queue is not empty
                    Close(fd_to_delete); // should always succeed since waiting is not empty here
                }
            }
            else if (strcmp(policy, "bf") == 0)
            {                                  // NOT A BUSY WAIT
                while (totalReqInQueue() != 0) // use a brand new spanking conditional variable to wait till all queues are empty
                {                              // waiting untill all the requests in the queues are handled
                    pthread_cond_wait(&empty_queue, &lock);
                }
                if (!is_vip)
                {                                // if req A is vip it shouldnt be dropped.
                    Close(connfd);               // after empty drop worker hoe req
                    pthread_mutex_unlock(&lock); // ------------------------------^
                    continue;
                }
            }
            else if (strcmp(policy, "random") == 0)
            {                                    // drop 50% of the waiting requests by random
                randomDequeue(waiting_requests); // should always succeed since waiting is not empty here
            }
        }

        // if we are here, there is place for new req:  assign the new req to proper queue
        if (is_vip)
        {                                                   // vip queue
            enqueue(vip_waiting_requests, connfd, arrival); // insert new vip req
            if (queueSize(vip_waiting_requests) == 1)
            { // if first vip req, signal there is pending vip req
                pthread_cond_signal(&vip_allowed);
            }
        }
        else
        { // worker queue
            enqueue(waiting_requests, connfd, arrival);
            if (queueEmpty(vip_waiting_requests) && !handeling_vip)
            {
                pthread_cond_signal(&worker_allowed);
            }
        }

        pthread_mutex_unlock(&lock); // ------------------------------^

        //
        // HW3: In general, don't handle the request in the main thread.
        // Save the relevant info in a buffer and have one of the worker threads
        // do the work.
    }
}

// connfd : connection file descriptor, represents the new client connection that the server has accepted

void initMaster(int threads_num, int queue_size, pthread_t *worker_threads, pthread_t *vip_thread)
{
    pthread_mutex_init(&lock, NULL);
    waiting_requests = queueCreate(queue_size);
    vip_waiting_requests = queueCreate(queue_size);
    handeling_requests = queueCreate(threads_num);
    pthread_cond_init(&new_req_allowed, NULL);
    pthread_cond_init(&worker_allowed, NULL);
    pthread_cond_init(&vip_allowed, NULL);
    pthread_cond_init(&empty_queue, NULL);
    pthread_cond_init(&full_queue, NULL);
    handeling_vip = 0;
    pthread_mutex_lock(&lock); // -------------------------------->

    int i;
    for (i = 0; i < threads_num; i++)
    {
        int *thread_num = (int *)malloc(sizeof(int));
        if (thread_num == NULL)
        {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }
        *thread_num = i;
        pthread_create(&worker_threads[i], NULL, workerThread, thread_num);
    }
    int *vip_thread_num = (int *)malloc(sizeof(int));
    if (vip_thread_num == NULL)
    {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    *vip_thread_num = i; // Last value of `i`
    pthread_create(vip_thread, NULL, vipThread, vip_thread_num);
    pthread_mutex_unlock(&lock); // ------------------------------^
}

void *workerThread(void *thread_number)
{
    // TODO: make sure these variables are local and not shared through threads
    struct timeval arrival, started, dispatch; // tis was modified
    threads_stats t_stats = (threads_stats)malloc(sizeof(struct Threads_stats));

    // Thread ID
    t_stats->id = *(int *)thread_number;
    free(thread_number);
    // Number of static requests
    t_stats->stat_req = 0;
    // Number of dynamic requests
    t_stats->dynm_req = 0;
    // Total number of requests
    t_stats->total_req = 0;

    while (1)
    {
        pthread_mutex_lock(&lock); // -------------------------------->
        if (DEBUG)
        {
            printf("[worker %d] : recieved a task at: %ld", t_stats->id, arrival.tv_sec);
        }

        while (queueEmpty(waiting_requests) || !queueEmpty(vip_waiting_requests) || handeling_vip)
        { // if empty or vip req, wait
            pthread_cond_wait(&worker_allowed, &lock);
        }
        // if I'm here, means there is worker request and I can handle it.
        // pop from waiting, insert to handeling:
        arrival = queueHeadArrivalTime(waiting_requests);
        int connfd = dequeue(waiting_requests);
        enqueue(handeling_requests, connfd, arrival);

        pthread_mutex_unlock(&lock); // ------------------------------^

        gettimeofday(&started, NULL);            // make sure its the difference between
        timersub(&started, &arrival, &dispatch); // dispatch = started - arrival

        int skip_invoked = requestHandle(connfd, arrival, dispatch, t_stats); // perfrom request outside of lock
        Close(connfd);

        pthread_mutex_lock(&lock); // -------------------------------->
        dequeueByReq(handeling_requests, connfd); // validate after handling A
        if (totalReqInQueue() < queue_size)
        { // this req is done. get another one by master
            pthread_cond_signal(&new_req_allowed);
        }
        if (totalReqInQueue() == 0)
        { // for block_flush
            pthread_cond_signal(&empty_queue);
        }

        int skip_connfd;
        if (skip_invoked)
        {                                                     // need to handle latest req by the sme thread
            arrival = queueHeadArrivalTime(waiting_requests); // to be changed
            skip_connfd = skipDequeue(waiting_requests);
            if (skip_connfd == -1)
            { // no new fd found
                skip_invoked = 0;
            }
            else
            {
                enqueue(handeling_requests, skip_connfd, arrival);
            }
            // do we want to change the fd of ourselves or does it not mean anything?
            // if error accures, notice handling_queue
        }
        pthread_mutex_unlock(&lock); // ------------------------------^

        if (skip_invoked)
        {                                            // not supposed to check reccursive skip
            gettimeofday(&started, NULL);            // make sure its the difference between
            timersub(&started, &arrival, &dispatch); // dispatch = started - arrival

            requestHandle(skip_connfd, arrival, dispatch, t_stats);
            Close(skip_connfd);
        }

        pthread_mutex_lock(&lock); // -------------------------------->
        if (DEBUG)
        {
            printf("[worker] : finished a task");
        }
        if (skip_invoked)
        {
            dequeueByReq(handeling_requests, skip_connfd);
            if (totalReqInQueue() < queue_size)
            { // this req is done. get another one by master
                pthread_cond_signal(&new_req_allowed);
            }
            if (totalReqInQueue() == 0)
            { // for block_flush
                pthread_cond_signal(&empty_queue);
            }
        }

        pthread_mutex_unlock(&lock); // ------------------------------^
    }
}

void *vipThread(void *thread_number)
{
    // TODO: make sure these variables are local and not shared through threads
    struct timeval arrival, started, dispatch; // tis was modified
    threads_stats t_stats = (threads_stats)malloc(sizeof(struct Threads_stats));

    // Thread ID
    t_stats->id = *(int *)thread_number;
    free(thread_number);
    // Number of static requests
    t_stats->stat_req = 0;
    // Number of dynamic requests
    t_stats->dynm_req = 0;
    // Total number of requests
    t_stats->total_req = 0;

    while (1)
    {
        pthread_mutex_lock(&lock); // -------------------------------->
        if (DEBUG)
        {
            printf("[vip] : recieved a task at: %ld", arrival.tv_sec);
        }

        while (queueEmpty(vip_waiting_requests))
        { // if no vip req, wait
            pthread_cond_wait(&vip_allowed, &lock);
        }
        handeling_vip = 1;
        // if I'm here, means there is vip request and I can handle it.
        arrival = queueHeadArrivalTime(vip_waiting_requests);
        int connfd = dequeue(vip_waiting_requests);

        pthread_mutex_unlock(&lock); // ------------------------------^

        gettimeofday(&started, NULL);            // make sure its the difference between
        timersub(&started, &arrival, &dispatch); // dispatch = started - arrival

        requestHandle(connfd, arrival, dispatch, t_stats); // perfrom request outside of lock
        Close(connfd);

        pthread_mutex_lock(&lock); // -------------------------------->
        handeling_vip = 0;
        if (DEBUG)
        {
            printf("[vip] : finished a task");
        }
        if (queueEmpty(vip_waiting_requests) && !queueEmpty(waiting_requests) && !handeling_vip)
        { // if no more vip - pull worker req
            pthread_cond_broadcast(&worker_allowed);
        }
        if (totalReqInQueue() < queue_size)
        { // this req is done. get another one by master
            pthread_cond_signal(&new_req_allowed);
        }
        if (totalReqInQueue() == 0)
        { // for block_flush
            pthread_cond_signal(&empty_queue);
        }

        pthread_mutex_unlock(&lock); // ------------------------------^
    }
}

int isValidPolicy(char *policy)
{
    for (int i = 0; i < NUM_OF_POLICIES; i++)
    {
        if (strcmp(common_policies[i], policy) == 0)
        {
            return 1;
        }
    }
    return 0;
}

// Usage: <port> <threads_num> <queue_size> <policy>
void getargs(int *port, int *threads_num, int *queue_size, char *policy, int argc, char *argv[])
{
    if (argc < 5)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *threads_num = atoi(argv[2]);
    *queue_size = atoi(argv[3]);

    if (*port <= 1024 || *threads_num <= 0 || *queue_size <= 0 || !isValidPolicy(argv[4]))
    { // argument validation
        exit(1);
    }
    strcpy(policy, argv[4]);
}

// just to make code more readable
int totalReqInQueue()
{
    return queueSize(vip_waiting_requests) + queueSize(waiting_requests) + queueSize(handeling_requests) + handeling_vip;
}

// return number of waiting requests
int totalWaitReqInQueue()
{
    return queueSize(vip_waiting_requests) + queueSize(waiting_requests);
}
