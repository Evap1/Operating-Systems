#include "segel.h"
#include "request.h"
#include "queue.h"
#include "pthread.h"

// 
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// DECLERATIONS
void initMaster();
void *workerThread();
void *vipThread();
void getargs(int *port, int* threads_num, int* queue_size, int argc, char *argv[]);
int totalReqInQueue();


// GLOBALS
Queue handeling_requests, waiting_requests, vip_waiting_requests;
pthread_mutex_t lock;
pthread_cond_t new_req_allowed, vip_allowed, worker_allowed;
int queue_size;


int main(int argc, char *argv[])
{

    int listenfd, connfd, port, clientlen, threads_num;
    struct sockaddr_in clientaddr;

    getargs(&port, &threads_num, &queue_size , argc, argv);
    pthread_t *worker_threads = malloc(sizeof(pthread_t) * threads_num);
    pthread_t vip_thread;
    initMaster(threads_num, queue_size, worker_threads, &vip_thread);

    // segel code ----- >
    // start listening
    listenfd = Open_listenfd(port);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        struct timeval arrival; // TO BE DELETED !!! JUST TO COMPILE FOR ENQUEUE

        pthread_mutex_lock(&lock); // -------------------------------->

        while (totalReqInQueue() >= queue_size){ // waiting for place in wait queue
            
            printf("[MASTER] Queue full! Waiting for space... (%d/%d)\n", totalReqInQueue(), queue_size);
            struct timeval wait_start, wait_end;
            gettimeofday(&wait_start, NULL);

            pthread_cond_wait(&new_req_allowed, &lock);

            gettimeofday(&wait_end, NULL);
            double wait_time = (wait_end.tv_sec - wait_start.tv_sec) + (wait_end.tv_usec - wait_start.tv_usec) / 1e6;
            printf("[MASTER] Resumed after waiting %.6f seconds.\n", wait_time);
        }

        // if we are here, there is place for new req:  assign the new req to proper queue
        if(getRequestMetaData(connfd)){ // vip queue
            enqueue(vip_waiting_requests, connfd, arrival); // insert new vip req
            if (queueSize(vip_waiting_requests) == 1 ){ // if first vip req, signal there is pending vip req
                printf("[MASTER] New VIP request! Notifying VIP thread.\n");

                pthread_cond_signal(&vip_allowed);
            }
        }
        else { // worker queue
            enqueue(waiting_requests, connfd, arrival);
            if (queueEmpty(vip_waiting_requests)){
                pthread_cond_signal(&worker_allowed);
            }
        }

        pthread_mutex_unlock(&lock); // ------------------------------^


        // 
        // HW3: In general, don't handle the request in the main thread.
        // Save the relevant info in a buffer and have one of the worker threads 
        // do the work. 
        // was: 
        //requestHandle(connfd);
        // segel code ----- ^

        // piazza fix (later for statistics) ----- >
        // arrival stores when the request arrives at the server.
        // dispatch stores when the request is assigned to a worker thread for processing.
        // t_stats stores statistics about a specific thread.
        // struct timeval arrival, dispatch;
        
        //threads_stats t_stats = (threads_stats)malloc(sizeof(struct Threads_stats));

        // pupper init just for fixing the bug
        // student code should init it properly by the thread
        // should read about timeval and see the def of the t_stats struct
        // Thread ID 
        /*
        t_stats->id = 0;
        // Number of static requests
        t_stats->stat_req = 0;
        // Number of dynamic requests
        t_stats->dynm_req = 0;
        // Total number of requests
        t_stats->total_req = 0;

        arrival.tv_sec = 0;
        arrival.tv_usec = 0;
        dispatch.tv_sec = 0;
        dispatch.tv_usec = 0;

        requestHandle(connfd, arrival, dispatch, t_stats);
        // piazza fix  ----- ^
        Close(connfd);
        */
    }
}

// connfd : connection file descriptor, represents the new client connection that the server has accepted


void initMaster(int threads_num, int queue_size, pthread_t *worker_threads, pthread_t vip_thread){
    pthread_mutex_init(&lock, NULL);
    waiting_requests = queueCreate(queue_size);
    vip_waiting_requests = queueCreate(queue_size);
    handeling_requests = queueCreate(threads_num);
    pthread_cond_init(&new_req_allowed, NULL);
    pthread_cond_init(&worker_allowed, NULL);
    pthread_cond_init(&vip_allowed, NULL);

    pthread_mutex_lock(&lock); // -------------------------------->
    for (int i = 0; i < threads_num; i++) {
        pthread_create(&worker_threads[i], NULL, workerThread, NULL);
    }
    pthread_create(&vip_thread, NULL, vipThread, NULL);
    pthread_mutex_unlock(&lock); // ------------------------------^
}

void *workerThread(){

    while(1) {
        pthread_mutex_lock(&lock); // -------------------------------->

        printf("[WORKER] Waiting for a request... Current Queue Size: %d\n", queueSize(waiting_requests));

        while(queueEmpty(waiting_requests) || !queueEmpty(vip_waiting_requests)){ // if empty or vip req, wait
            if (!queueEmpty(vip_waiting_requests)) printf("[WORKER] VIP request is pending. Blocking worker thread.\n");

            pthread_cond_wait(&worker_allowed, &lock);
            printf("[WORKER] woeker allowed, working queue: (%d/%d)\n", queueSize(waiting_requests)+ queueSize(handeling_requests), queue_size);
        }
        // if I'm here, means there is worker request and I can handle it.
        // pop from waiting, insert to handeling:
        struct timeval arrival = queueHeadArrivalTime(waiting_requests);
        int connfd = dequeue(waiting_requests);
        enqueue(handeling_requests, connfd, arrival);

        printf("[WORKER] Processing request %d, queue size: %d\n", connfd, queueSize(handeling_requests));

        pthread_mutex_unlock(&lock); // ------------------------------^

        struct timeval dispatch; // TO BE DELETED !!! JUST TO COMPILE FOR ENQUEUE
        threads_stats t_stats = (threads_stats)malloc(sizeof(struct Threads_stats));

        // pupper init just for fixing the bug
        // student code should init it properly by the thread
        // should read about timeval and see the def of the t_stats struct
        // Thread ID 
        t_stats->id = 0;
        // Number of static requests
        t_stats->stat_req = 0;
        // Number of dynamic requests
        t_stats->dynm_req = 0;
        // Total number of requests
        t_stats->total_req = 0;

        arrival.tv_sec = 0;
        arrival.tv_usec = 0;
        dispatch.tv_sec = 0;
        dispatch.tv_usec = 0;
        // TO BE DELETED

        requestHandle(connfd, arrival, dispatch, t_stats); // perfrom request outside of lock
        Close(connfd);

        pthread_mutex_lock(&lock); // -------------------------------->
        dequeueByReq(handeling_requests, connfd);
        if (totalReqInQueue() < queue_size){ // this req is done. get another one by master
            pthread_cond_signal(&new_req_allowed);
        }
        pthread_mutex_unlock(&lock); // ------------------------------^
    }
}

void *vipThread(){

    while(1){
        pthread_mutex_lock(&lock); // -------------------------------->
        
        printf("[VIP] Waiting for VIP request... Current Queue Size: %d\n", queueSize(vip_waiting_requests));

        while(queueEmpty(vip_waiting_requests)){ // if no vip req, wait
            pthread_cond_wait(&vip_allowed, &lock);
        }
        // if I'm here, means there is vip request and I can handle it.
        struct timeval arrival = queueHeadArrivalTime(vip_waiting_requests);
        int connfd = headDesciprot(vip_waiting_requests);

        printf("[VIP] Processing VIP request %d, queue size: %d\n", connfd, queueSize(vip_waiting_requests));

        pthread_mutex_unlock(&lock); // ------------------------------^

        struct timeval dispatch; // TO BE DELETED !!! JUST TO COMPILE FOR ENQUEUE
        threads_stats t_stats = (threads_stats)malloc(sizeof(struct Threads_stats));

        // pupper init just for fixing the bug
        // student code should init it properly by the thread
        // should read about timeval and see the def of the t_stats struct
        // Thread ID 
        t_stats->id = 0;
        // Number of static requests
        t_stats->stat_req = 0;
        // Number of dynamic requests
        t_stats->dynm_req = 0;
        // Total number of requests
        t_stats->total_req = 0;

        arrival.tv_sec = 0;
        arrival.tv_usec = 0;
        dispatch.tv_sec = 0;
        dispatch.tv_usec = 0;
        // TO BE DELETED
        requestHandle(connfd, arrival, dispatch, t_stats); // perfrom request outside of lock
        Close(connfd);

        pthread_mutex_lock(&lock); // -------------------------------->
        dequeue(vip_waiting_requests);
        if (queueEmpty(vip_waiting_requests) && !queueEmpty(waiting_requests)){ // if no more vip - pull worker req
            pthread_cond_broadcast(&worker_allowed);
        }
        if (totalReqInQueue() < queue_size){ // this req is done. get another one by master
            pthread_cond_signal(&new_req_allowed);
        }
        pthread_mutex_unlock(&lock); // ------------------------------^
    }
}

void getargs(int *port, int* threads_num, int* queue_size, int argc, char *argv[])
{
    if (argc < 4) {
	    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
	    exit(1);
    }
    *port = atoi(argv[1]);
    *threads_num = atoi(argv[2]);
    *queue_size = atoi(argv[3]);

    if (*port <= 1024 || *threads_num <= 0 || *queue_size <= 0){ // argument validation
        exit(1);
    }
}

// just to make code more readable
int totalReqInQueue(){
    return queueSize(vip_waiting_requests) + queueSize(waiting_requests) + queueSize(handeling_requests);
}
