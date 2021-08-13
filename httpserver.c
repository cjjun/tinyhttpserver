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

//  ./httpserver --files files/ --port 8000 --num-threads 5
// ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 --num-threads 5
int main (int argc, char **argv) {
    server_port = 8000;
    for (int i = 1; i < argc; i++) {
        if (strcmp("--files", argv[i]) == 0) {
            if ( server_mode == PROXY ) {
                printf("Conflict option: --files\n");
                exit (-1);
            }
            if ( strncmp (argv[++i], "files/", 6) == 0 ) {
                server_mode = LOCAL;
                url = &argv[i][5];
            }
            else {
                printf("Illegal file path.\n");
                exit (-1);
            }
        } else if ( strcmp("--proxy", argv[i]) == 0){
            if ( server_mode == LOCAL ) {
                printf("Conflict option: --proxy\n");
                exit (-1);
            }
            server_mode = PROXY;
            url = argv[++i];
        } else if ( strcmp("--port", argv[i]) == 0 ) {
            server_port = atoi ( argv[++i] );
            if (server_port == 0) {
                printf("Ileggel port %s.\n", argv[i]);
                exit (-1);
            }
        } else if ( strcmp("--num-threads", argv[i]) == 0 ) {
            num_threads = atoi( argv[++i] );
            if (num_threads == 0) {
                printf("Ileggel num-threads %s.\n", argv[i]);
                exit (-1);
            }
        } else if ( strcmp("--help", argv[i]) == 0 ) {
            printf("\nUsage:  ./httpserver --files files/ --port 8000 [--num-threads 5] or\n"
            "\t./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n\n");
            exit (0);
        } else {
            printf("undefined parameter %s\n", argv[i]);
            exit (-1);
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
        
}