#include "segel.h"
#include "request.h"

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
void getargs(int *port, int argc, char *argv[])
{
    if (argc < 2) {
	fprintf(stderr, "Usage: %s <port>\n", argv[0]);
	exit(1);
    }
    *port = atoi(argv[1]);
}

int main(int argc, char *argv[])
{

    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;

    getargs(&port, argc, argv);

    // 
    // HW3: Create some threads...
    //
    // piazza fix :
    struct timeval arrival, dispatch;
    threads_stats t_stats = (threads_stats)malloc(sizeof(struct Threads_stats));

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
	// 
	// HW3: In general, don't handle the request in the main thread.
	// Save the relevant info in a buffer and have one of the worker threads 
	// do the work. 
	// was: 
	//requestHandle(connfd);
    // piazza pix:
    requestHandle(connfd, arrival, dispatch, t_stats);
	Close(connfd);
    }

}