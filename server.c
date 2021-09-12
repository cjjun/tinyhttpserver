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
#include<sys/types.h>
#include<sys/stat.h>
#include<netdb.h>
#include<dirent.h>
#include<unistd.h>
#include <time.h>

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
    int len_domain;
    for (len_domain = 0; url[len_domain] != ':' && url[len_domain]; len_domain++);

    char *domain = malloc( len_domain + 1 );
    strlcpy(domain, url, len_domain + 1);
    domain[len_domain] = '\0';


    if ( url[len_domain] == ':') {
        proxy_port = atoi (url + len_domain + 1);
        if (proxy_port == 0) {
            perror("Illegal port");
            exit(EXIT_FAILURE);
        }
    }
    else 
        proxy_port = 80;
    printf("proxy %s:%d\n", domain, proxy_port);
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

int isDir (char *path) {
    struct stat path_stat;
    if (stat(path, &path_stat) == -1)
        return -1;
    return S_ISDIR(path_stat.st_mode);
}

char *get_file_suffix (char *filename) {
    char *p = NULL;
    for (int i = 0; filename[i]; i++) 
        if (filename[i] == '.')
            p = &filename[i];
    if(p)
        return p + 1;
    else 
        return p;
}

void http_send_file (int fd, char *filename) {
    // Get current time
    char *buf = malloc(LIBHTTP_REQUEST_MAX_SIZE+1);
    time_t now = time (0);
    strftime (buf, 100, "%c", localtime (&now));

    struct FILE *file = fopen(filename, "r");
    fseek(file, 0, SEEK_END);
    int lengthOfFile = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Send http header
    http_start_response(fd, 200);
    http_send_header(fd, "Server", "Tinyhttpserver");
    http_send_header(fd, "Date", buf);
    char *suffix = get_file_suffix(filename);
    if (strcmp(suffix, "html") == 0)
        http_send_header(fd, "Content-type", "text/html; charset=utf-8"); 
    else if (strcmp(suffix, "pdf") == 0) 
        http_send_header(fd, "Content-type", "application/pdf"); 
    else 
        http_send_header(fd, "Content-type", "application/octet-stream"); 
    sprintf(buf, "%d", lengthOfFile);
    http_send_header(fd, "Content-Length", buf);
    http_end_headers(fd);


    while (lengthOfFile > 0) {
        int readbytes = fread(buf, 1, LIBHTTP_REQUEST_MAX_SIZE > lengthOfFile? 
            lengthOfFile: LIBHTTP_REQUEST_MAX_SIZE, file );
        http_send_data(fd, buf, readbytes);
        lengthOfFile -= readbytes;
    }

    fclose (file);
    free (buf);
}

void http_send_404 (int fd) {
    char buf[100];
    http_start_response(fd, 404);
    http_send_header(fd, "Server", "Tinyhttpserver");
    time_t now = time (0);
    strftime (buf, 100, "%c", localtime (&now));
    http_send_header(fd, "Date", buf);
    http_send_header(fd, "Connection", "close");
    http_send_header(fd, "Content-type", "text/html; charset=utf-8"); 
    sprintf(buf, "%d", 0);
    http_send_header(fd, "Content-Length", buf);

    http_end_headers(fd);
}

// Concert %AB to uint8_t 0xAB and return as character
char read_perc (char *p) {
    uint8_t ans = 0;
    if (p[1] >= 'A' && p[1] <= 'F')
        ans = p[1] - 'A' + 10;
    else 
        ans = p[1] - '0';
    
    ans <<= 4;

    if (p[2] >= 'A' && p[2] <= 'F')
        ans += p[2] - 'A' + 10;
    else 
        ans += p[2] - '0';
    return ans;
}

// decode url with percentage encode
void urlDecode(char *src, char *des) {
    int top = 0;
    for (int i = 0; src[i]; i++) {
        if (src[i] == '%') {
            des[top++] = read_perc(src + i);
            i += 2;
        }
        else 
            des[top++] = src[i];
    }
    des[top] = '\0';
}

void handle_web_request (int fd) {
    struct http_request *request = http_request_parse(fd);
    printf("\"GET %s\"", request->path);

    char *old = request->path;
    request->path = malloc( strlen(old) + 10 );
    request->path[0] = '.';
    urlDecode(old, request->path+1);

    free (old);
    int isdir = isDir(request->path);
    if (isdir == -1) {  
        http_send_404(fd);
        printf(" 404 Not found\n");
        return;
    }
    
    if (isdir) {
        struct DIR *dir = opendir(request->path);
        struct dirent *entry;
        
        char buf[50];
        sprintf(buf, "/tmp/tinyhttp_%d.html", fd);
        struct FILE *file = fopen(buf, "w");
        // Generate html head
        fprintf(file, "%s", "<!DOCTYPE HTML PUBLIC>\n");
        fprintf(file, "%s", "<html>\n");
            fprintf(file, "%s", "<head>\n");
                fprintf(file, "%s", "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n");
                fprintf(file, "%s", "<title>Directory listing for /</title>\n");
            fprintf(file, "%s", "</head>\n");

            fprintf(file, "%s", "<body>\n");
            fprintf(file, "%s", "<h1>Directory listing for /</h1>\n<hr>\n<ul>\n");
            while( (entry = readdir(dir)) != NULL ) {
                if (entry->d_name[0] == '.')
                    continue;
                if (entry->d_type & DT_DIR)
                    fprintf(file, "<li><a href=\"%s/\">%s/</a></li>\n", entry->d_name, entry->d_name);
                else 
                    fprintf(file, "<li><a href=\"%s\">%s</a></li>\n", entry->d_name, entry->d_name);
            }
            fprintf(file, "%s", "</ul>\n<hr>\n");
            fprintf(file, "%s", "</body>\n");
        fprintf(file, "%s", "</html>\n");

        fclose(file);
        closedir(dir);
        // Send and remove temporary file
        http_send_file(fd, buf);
        remove (buf);

    } else {
        http_send_file(fd, request->path);
    }

    printf(" 200 OK\n");
    free(request->path);
    free(request->method);
    free(request);
}