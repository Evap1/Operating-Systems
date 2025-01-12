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
    void initMaster(int threads_num, int queue_size, pthread_t *worker_threads, pthread_t* vip_thread);
    void *workerThread();
    void *vipThread();
    void getargs(int *port, int *threads_num, int *queue_size, char* policy, int argc, char *argv[]);
    int totalReqInQueue();
    bool isValidPolicy(char* policy);

    // GLOBALS
    Queue handeling_requests, waiting_requests, vip_waiting_requests;
    pthread_mutex_t lock;
    pthread_cond_t new_req_allowed, vip_allowed, worker_allowed, empty_queue, full_queue;
    int queue_size;
    int handeling_vip;
    const char* common_policies[] = {"block", "dh", "dt", "bf", "random"};

    
    int main(int argc, char *argv[])
    {
        int listenfd, connfd, port, clientlen, threads_num;
        char policy[BUFFER];
        struct sockaddr_in clientaddr;

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

            struct timeval arrival; // TO BE MODIFIED !!! JUST TO COMPILE FOR ENQUEUE

            pthread_mutex_lock(&lock); // -------------------------------->
            
            //proccess full queue according to policy
            if (totalReqInQueue() >= queue_size) {
                if ((sizeQueue(vip_waiting_requests) == queue_size) || strcmp(policy,"block") == 0) {   //as it wassssssss
                    while (totalReqInQueue() >= queue_size)
                    { // waiting for place in wait queue
                        pthread_cond_wait(&new_req_allowed, &lock);
                    }
                }
                else if (strcmp(policy,"dt") == 0) {                //drop the request and leaveeeee
                    Close(connfd);
                    pthread_mutex_unlock(&lock);
                    continue;
                }
                else if (strcmp(policy,"dh") == 0) {                //drop latest request in queue
                    int fd_to_delete = dequeue(waiting_requests);
                    if (fd_to_delete != -1) {
                    Close(fd_to_delete); 
                    }
                }
                else if (strcmp(policy,"bf") == 0) {                //NOT A BUSY WAIT
                    while (totalReqInQueue() != 0)                  //use a brand new spanking conditional variable to wait till queue is empty
                    { // waiting for place in wait queue
                        pthread_cond_wait(&empty_queue, &lock);
                    }
                    if(!getRequestMetaData(connfd)){                // if req A is vip it shouldnt be droped.
                        Close(connfd);                              // after empty drop worker hoe req
                        pthread_mutex_unlock(&lock);
                        continue;
                    }
                }
                else if (strcmp(policy,"random") == 0) {            //drop 50% of the waiting requests by random

                    int num_of_waiting_req = queueSize(waiting_requests);
                    int i = num_of_waiting_req;

                    while (num_of_waiting_req <= i / 2) {
                        int j = rand() % (num_of_waiting_req);  //generate a number between 0 and the total wait queue size

                        int fd_to_delete = dequeueByIndex(waiting_requests, j);
                        Close(fd_to_delete);
                        num_of_waiting_req--;
                    }
                }
            }

            // if we are here, there is place for new req:  assign the new req to proper queue
            if (getRequestMetaData(connfd))
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

    void initMaster(int threads_num, int queue_size, pthread_t *worker_threads, pthread_t* vip_thread)
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
        for (int i = 0; i < threads_num; i++)
        {
            pthread_create(&worker_threads[i], NULL, workerThread, NULL);
        }
        pthread_create(vip_thread, NULL, vipThread, NULL);
        pthread_mutex_unlock(&lock); // ------------------------------^
    }

    void *workerThread()
    {

        while (1)
        {
            pthread_mutex_lock(&lock); // -------------------------------->
            while (queueEmpty(waiting_requests) || !queueEmpty(vip_waiting_requests) || handeling_vip)
            { // if empty or vip req, wait
                pthread_cond_wait(&worker_allowed, &lock);
            }
            // if I'm here, means there is worker request and I can handle it.
            // pop from waiting, insert to handeling:
            struct timeval arrival = queueHeadArrivalTime(waiting_requests);
            int connfd = dequeue(waiting_requests);
            enqueue(handeling_requests, connfd, arrival);

            pthread_mutex_unlock(&lock); // ------------------------------^

            // TODO: make sure these variables are local and not shared through threads
            struct timeval dispatch; // TO BE MODIFIEF !!! JUST TO COMPILE FOR ENQUEUE
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

           int skip_invoked = requestHandle(connfd, arrival, dispatch, t_stats); // perfrom request outside of lock
            Close(connfd);

            pthread_mutex_lock(&lock); // -------------------------------->
            int skip_connfd;
            if(skip_invoked) { // need to handle latest req by the sme thread
                arrival = queueHeadArrivalTime(waiting_requests); // to be changed
                skip_connfd = skipDequeue(waiting_requests);
                if (skip_connfd == -1) { //no new fd found
                    skip_invoked = 0;
                }
                else {
                    enqueue(handeling_requests, skip_connfd, arrival);
                }
                // do we want to change the fd of ourselves or does it not mean anything?
                // if error accures, notice handling_queue
            }
            pthread_mutex_unlock(&lock); // ------------------------------^

            if(skip_invoked) {      //not supposed to check reccursive skip
                requestHandle(skip_connfd, arrival, dispatch, t_stats);
                Close(skip_connfd);
            }

            pthread_mutex_lock(&lock); // --------------------------------> 
            dequeueByReq(handeling_requests, connfd);
            if (skip_invoked){
                dequeueByReq(handeling_requests, skip_connfd);
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

    void *vipThread()
    {

        while (1)
        {
            pthread_mutex_lock(&lock); // -------------------------------->

            while (queueEmpty(vip_waiting_requests))
            { // if no vip req, wait
                pthread_cond_wait(&vip_allowed, &lock);
            }
            handeling_vip = 1;
            // if I'm here, means there is vip request and I can handle it.
            struct timeval arrival = queueHeadArrivalTime(vip_waiting_requests);
            int connfd = dequeue(vip_waiting_requests);

            pthread_mutex_unlock(&lock); // ------------------------------^

            struct timeval dispatch; // TO BE MODIFIED !!! JUST TO COMPILE FOR ENQUEUE
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
            handeling_vip = 0;
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

    bool isValidPolicy(char* policy) {
        for(int i = 0; i < NUM_OF_POLICIES; i++) {
            if (strcmp(common_policies[i], policy) == 0) {
                return true;
            }
        }
        return false;
    }

    void getargs(int *port, int *threads_num, int *queue_size, char* policy, int argc, char *argv[])
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

