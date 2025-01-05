#include "segel.h"
#include "request.h"
#include "queue.h"
#include <pthread.h>


int DEBUG = 0;

int num_of_vip = 0;
pthread_mutex_t lock;
pthread_cond_t cond;
//pthread_cond_t cond_stop;

Queue regular_threads = NULL;
Queue vip_threads = NULL;
// 
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// HW3: Parse the new arguments too
void getargs(int *port,  int argc, char *argv[], int *thread_num, int *queue_size)
{
    if (argc < 2) {
	fprintf(stderr, "Usage: %s <port>\n", argv[0]);
	exit(1);
    }
    //added
    else if (argc == 2) {
        *port = atoi(argv[1]);
        *thread_num = 1;
        *queue_size = 1;
    }
    else if((argc > 2 && argc < 4) || argc > 4) {
        fprintf(stderr, "Usage: %s <port> <num of threads> <queue size>\n", argv[0]);
	    exit(1);
    }
    *port = atoi(argv[1]);
    *thread_num = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
}

void* thread_main(void* args) {

    while(1) {
        pthread_mutex_lock(&lock);
        /*while(queue_empty(regular_threads)){
            pthread_cond_wait(&cond, &lock);        //need to make a signal somwhere
        }*/
        //some cool code here
        
        pthread_mutex_unlock(&lock);
    }
}

int main(int argc, char *argv[])
{
                                            //added
    int listenfd, connfd, port, clientlen, queue_max_size, threads_num;
    struct sockaddr_in clientaddr;

    getargs(&port, argc, argv , &threads_num, &queue_max_size);

    // 
    // HW3: Create some threads...
    //
    // piazza fix :
    struct timeval arrival, dispatch;
    threads_stats t_stats = (threads_stats)malloc(sizeof(struct Threads_stats));

    //queues create
    /*regular_threads = queue_create(queue_max_size);
    vip_threads = queue_create(threads_num);*/

    // pupper init just for fixing the bug
    // student code should init it properly by the thread
    // should read about timeval and see the def of the t_stats struct
    t_stats->id = 0;
    t_stats->stat_req = 0;
    t_stats->dynm_req = 0;
    t_stats->total_req = 0;
    arrival.tv_sec = 0;
    arrival.tv_usec = 0;
    dispatch.tv_sec = 0;
    dispatch.tv_usec =0;

    // segel code from now on:
    listenfd = Open_listenfd(port);
    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

    /*pthread_mutex_lock(&lock);

        //some cool code here

    pthread_mutex_unlock(&lock);*/

	// 
	// HW3: In general, don't handle the request in the main thread.
	// Save the relevant info in a buffer and have one of the worker threads 
	// do the work. 
	// was: 
	//requestHandle(connfd);
    // piazza pix:
    if (!getRequestMetaData(connfd) && num_of_vip) {           //if realtime event
    }
    requestHandle(connfd, arrival, dispatch, t_stats);
	Close(connfd);
    }

}



    


 
