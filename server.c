#include "server.h"
#include "threadpool.h"
#include "libhttp.h"

#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>       /* Basic net operations */
#include<sys/socket.h>     /* Socket lib */
#include<netdb.h>

#define LIBHTTP_REQUEST_MAX_SIZE 8192

extern char *url;

struct pipe {
    int src_fd, des_fd;

};

void proxy_back (void *aux) {
    struct pipe *pipe = (struct pipe *)aux;
    int remote_fd = pipe->src_fd;
    int client_fd = pipe->des_fd;

    char *buffer = malloc (LIBHTTP_REQUEST_MAX_SIZE + 1);

    int readbytes;

    while ( (readbytes = read (remote_fd, buffer, LIBHTTP_REQUEST_MAX_SIZE) ) > 0 ) {
        buffer[readbytes] = '\0';
        write (client_fd, buffer, readbytes);
    }
    printf("Remote %d has closed connection\n", remote_fd);
    close (remote_fd);
    free (buffer);
    free (pipe);
}

void handle_proxy_request(int fd) {
    
    printf("Client %d has connected\n", fd); 

    int remote_fd;
    int proxy_port;
    struct sockaddr_in remote_address;
    int len_domain = strrchr(url, ':') - url;

    char *domain = malloc( len_domain + 1 );
    strncpy(domain, url, len_domain);
    domain[len_domain] = '\0';


    if ( domain[len_domain] == ':') {
        proxy_port = atoi (url + len_domain + 1);
        if (proxy_port == 0) {
            perror("Illegal port");
            exit(EXIT_FAILURE);
        }
    }
    else 
        proxy_port = 80;

    struct hostent *host =  gethostbyname (domain);
    free (domain);
    if (host == NULL) {
        perror("unknown domain");
        exit(EXIT_FAILURE);
    }

    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons (proxy_port);
    remote_address.sin_addr.s_addr = *(in_addr_t *)host->h_addr_list[0];
    
    if ( (remote_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0 ) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    unsigned int value = 1;
    setsockopt(remote_fd, SOL_SOCKET, SO_REUSEADDR,(void *) &value,sizeof(value));

    if ( connect (remote_fd, (struct sockaddr *)&remote_address, sizeof (remote_address)) < 0 ) {
        perror("connect failed");
        exit(EXIT_FAILURE);
    }
    printf("%d --> %d connected\n", fd, remote_fd); 
    struct pipe *pipe = malloc( sizeof(struct pipe) );
    *pipe = (struct pipe){remote_fd, fd};

    executor_t recv_executor = executor_init (proxy_back, pipe);
    if (recv_executor == EID_ERROR) {
        perror("create receive task failed\n");
        exit(EXIT_FAILURE);
    }
   if( !executor_start (recv_executor) ) {
       perror("fail to start receive task\n");
        exit(EXIT_FAILURE);
   }

    char *buffer = malloc (LIBHTTP_REQUEST_MAX_SIZE + 1);
    int readbytes;
    while ( (readbytes = read (fd, buffer, LIBHTTP_REQUEST_MAX_SIZE) ) > 0 ) {
        buffer[readbytes] = '\0';
        write (remote_fd, buffer, readbytes);
    }
    free (buffer);
    printf("Client %d has closed connection\n", fd);

    close (fd);

}