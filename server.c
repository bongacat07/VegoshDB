#include "netUtils.h"
#include "protocol.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

/**
 * @brief Initializes a TCP server on port 8080 and handles clients sequentially.
 *
 * The server:
 *   - Creates a listening socket
 *   - Binds to INADDR_ANY:8080
 *   - Listens with a backlog of 128
 *   - Accepts clients in a loop
 *   - For each connection, repeatedly calls parser() until the client disconnects
 *
 * Note:
 *   This is a single-threaded, iterative server (one client handled at a time).
 */
int startServer() {
    int connfd, listenfd;
    struct sockaddr_in clientaddr, servaddr;
    socklen_t clilen;

    /* Create a TCP socket (IPv4, stream-oriented). */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        perror("Socket Error");
        return -1;
    }

    /* Allow quick reuse of the address after server restart. */
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Zero out the server address structure before filling it. */
    memset(&servaddr, 0, sizeof(servaddr));

    /* Configure the server to listen on all interfaces, port 8080. */
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(8080);

    /* Bind the socket to the specified address and port. */
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("Bind Error");
        return -1;
    }

    /* Mark the socket as passive with a backlog queue of 128. */
    if (listen(listenfd, 128) == -1) {
        perror("Listen Error");
        return -1;
    }

    /* Prevent zombie processes from forked children. */
    signal(SIGCHLD, SIG_IGN);

    /* Main accept loop: runs for the lifetime of the server. */
    for (;;) {
        clilen = sizeof(clientaddr);

        /* Block until a new client connection arrives. */
        connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clilen);
        if (connfd == -1) {
            perror("Accept Error");
            continue; /* don't kill the server on a single bad accept */
        }

        /* Fork a child process to handle this client. */
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(connfd);
            continue;
        }

        if (pid == 0) {
            /* Child process: handle the client connection. */

            close(listenfd); /* child does not accept new clients */

            printf("Accepted a new connection\n");

            /*
             * Process requests from this client.
             * parser() handles one protocol command per call and
             * returns 0 while the connection should remain open.
             */
            while (parser(connfd) == 0)
                ;

            /* Client session finished — close the connected socket. */
            close(connfd);
            printf("Connection closed\n");

            _exit(0); /* terminate child process safely */
        }

        /* Parent process: close connected socket and continue accepting. */
        close(connfd);
    }
}
