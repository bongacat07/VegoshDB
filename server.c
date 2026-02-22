#include "netUtils.h"
#include "protocol.h"

//This file contains the implementation of the server
//

int startServer() {
    int connfd, listenfd;
    struct sockaddr_in clientaddr, servaddr;
    socklen_t clilen;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        perror("Socket Error");
        return -1;
    }
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(8080);

    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("Bind Error");
        return -1;
    }

    if (listen(listenfd, 128) == -1) {
        perror("Listen Error");
        return -1;
    }

    for (;;) {
        clilen = sizeof(clientaddr);
        connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clilen);
        if (connfd == -1) {
            perror("Accept Error");
            return -1;
        }
        printf("Accepted a new connection\n");
       parser(connfd);
    }
}
