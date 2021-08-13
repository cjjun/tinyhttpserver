#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<math.h>
#include<ctype.h>
#include<string.h>
#include<dirent.h>          /* Directory lib */
#include<arpa/inet.h>       /* Basic net operations */
#include <sys/socket.h>     /* Socket lib */
/* User lib */
#include "threadpool.h"
#include "libhttp.h"
#include "server.h"

/* Global variables */
int server_port = 8000;
int num_threads = 10;
char *url = NULL;
enum server_mode server_mode = UNDEFINED;

// Entry function for client file requests.
void serve_client (void *aux) {
    int client_fd = (int)aux;
}

// Entry function to proxy client requests.
void proxy_client (void *aux) {
    int client_fd = (int)aux;
}

void run_server (void) {
    int server_fd, client_fd;
    struct sockaddr_in address;

    pool_init (num_threads);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons (server_port);

    if ( (server_fd = socket (AF_INET, SOCK_STREAM, 0) ) == 0 ) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    if (bind (server_fd, (struct sockeaddr *)&address, sizeof(address) ) < 0 ) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if ( listen (server_fd, 2000) == -1) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    socklen_t addrlen = sizeof(address);

    while (true) {
        client_fd = accept (server_fd, (struct sockeaddr *)&address, &addrlen );
        if (client_fd == -1) {
            perror ("Error accepting socket\n");
            continue;
        }

        executor_t client_executor = executor_init ( server_mode == LOCAL? 
                                serve_client: proxy_client,  (void *)client_fd);
        executor_start (&client_executor);
    }
}

//  ./httpserver --files files/ --port 8000 --num-threads 5
// ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 --num-threads 5
int main (int argc, char **argv) {
    server_port = 8000;
    for (int i = 1; i < argc; i++) {
        if (strcmp("--files", argv[i]) == 0) {
            if ( server_mode == PROXY ) {
                printf("Conflict option: --files\n");
                exit (EXIT_FAILURE);
            }
            if ( strncmp (argv[++i], "files/", 6) == 0 ) {
                server_mode = LOCAL;
                url = &argv[i][5];
            }
            else {
                printf("Illegal file path.\n");
                exit (EXIT_FAILURE);
            }
        } else if ( strcmp("--proxy", argv[i]) == 0){
            if ( server_mode == LOCAL ) {
                printf("Conflict option: --proxy\n");
                exit (EXIT_FAILURE);
            }
            server_mode = PROXY;
            url = argv[++i];
        } else if ( strcmp("--port", argv[i]) == 0 ) {
            server_port = atoi ( argv[++i] );
            if (server_port == 0 || server_port < 0 || server_port > 65535) {
                printf("Ileggel port %s.\n", argv[i]);
                exit (EXIT_FAILURE);
            }
        } else if ( strcmp("--num-threads", argv[i]) == 0 ) {
            num_threads = atoi( argv[++i] );
            if (num_threads == 0) {
                printf("Ileggel num-threads %s.\n", argv[i]);
                exit (EXIT_FAILURE);
            }
        } else if ( strcmp("--help", argv[i]) == 0 ) {
            printf("\nUsage:  ./httpserver --files files/ --port 8000 [--num-threads 5] or\n"
            "\t./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n\n");
            exit (0);
        } else {
            printf("undefined parameter %s\n", argv[i]);
            exit (EXIT_FAILURE);
        }
    }
    printf("Check parameters...\n");
    assert (server_mode != UNDEFINED);
    if (server_mode == PROXY)
        printf("Mode PROXY, url = %s\n", url);
    else 
        printf("Mode LOCAL, url = %s\n", url);
    
    if (server_port != 0)
        printf("Sever works at port %d\n", server_port);
    
    if (num_threads != 0)
        printf("Thread pool is initialized with size %d\n", num_threads);
    
    run_server ();
}