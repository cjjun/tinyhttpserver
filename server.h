#ifndef __SERVER_H
#define __SERVER_H

enum server_mode{
    UNDEFINED,
    LOCAL,
    PROXY
};

void handle_proxy_request(int fd);
void handle_web_request (int fd);

#endif /* server.c */