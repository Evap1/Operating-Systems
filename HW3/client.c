#include "segel.h"
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define TOTAL_REQUESTS 10 // Number of requests to send
#define SERVER_PORT 8003   // Adjust if needed
#define SERVER_HOST "localhost"

typedef struct {
    char *method;
    char *filename;
} ClientRequest;

/*
 * Send an HTTP request for the specified file 
 */
void clientSend(int fd, char *filename, char* method)
{
  char buf[MAXLINE];
  char hostname[MAXLINE];

  Gethostname(hostname, MAXLINE);
  sprintf(buf, "%s %s HTTP/1.1\n", method, filename);
  sprintf(buf, "%shost: %s\n\r\n", buf, hostname);
  printf("Sending: %s\n", buf);
  Rio_writen(fd, buf, strlen(buf));
}
  
/*
 * Read the HTTP response and print it out
 */
void clientPrint(int fd)
{
  rio_t rio;
  char buf[MAXBUF];  
  int n;
  
  Rio_readinitb(&rio, fd);
  n = Rio_readlineb(&rio, buf, MAXBUF);

  while (strcmp(buf, "\r\n") && (n > 0)) {
    printf("Header: %s", buf);
    n = Rio_readlineb(&rio, buf, MAXBUF);
  }
}

void *send_request(void *arg) {
    ClientRequest *req = (ClientRequest *)arg;
    int clientfd = Open_clientfd(SERVER_HOST, SERVER_PORT);
    printf("clientfd = %d\n", clientfd);

    if (clientfd < 0) {
        printf("[ERROR] Client failed to connect!\n");
        return NULL;
    }

    printf("[TEST] Sending %s request: %s\n", req->method, req->filename);
    clientSend(clientfd, req->filename, req->method);
    clientPrint(clientfd);

    Close(clientfd);
    return NULL;
}

void runServerTest() {
    pthread_t clients[TOTAL_REQUESTS];

    // Requests to send in sequence
    ClientRequest requests[TOTAL_REQUESTS] = {
        {"GET", "/home.html"},   // Worker
        {"GET", "/home.html"},   // Worker
        {"REAL", "/home.html"},  // VIP
        {"GET", "/home.html"},   // Worker (Blocked)
        {"REAL", "/home.html"},  // VIP
        {"GET", "/home.html"},   // Worker (Blocked)
        {"REAL", "/home.html"},  // VIP
        {"GET", "/home.html"},   // Worker (Blocked)
        {"GET", "/home.html"},   // Worker (Blocked)
        {"REAL", "/home.html"}   // VIP
    };

    // Launch all clients
    for (int i = 0; i < TOTAL_REQUESTS; i++) {
        pthread_create(&clients[i], NULL, send_request, (void *)&requests[i]);
    }

    // Wait for all clients to finish
    for (int i = 0; i < TOTAL_REQUESTS; i++) {
        pthread_join(clients[i], NULL);
    }

    printf("\n✅ [TEST] All client requests completed successfully.\n\n");
}

int main(int argc, char *argv[]) {
    printf("\n=== Running Automated Server Tests ===\n");
    runServerTest();
    return 0;
}
