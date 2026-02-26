#include <stdio.h>
#include <string.h>
#include "server.h"
#include "client.h"
#include "vegosh.h"
#define DEFAULT_IP "127.0.0.1"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: vegosh <client [ip_address]|server>\n");
        return 1;
    }

    if (strcmp(argv[1], "client") == 0) {
        const char *ip = (argc >= 3 && argv[2] != NULL) ? argv[2] : DEFAULT_IP;
        printf("Starting client, connecting to %s...\n", ip);
        if (startClient(ip) == -1) {
            fprintf(stderr, "startClient failed\n");
            return 1;
        }
        printf("Client disconnected.\n");
    } else if (strcmp(argv[1], "server") == 0) {
        printf("Starting server on port 8080...\n");
        initializevegosh();
        printf("DB initialized. Waiting for connections...\n");
        if (startServer() == -1) {
            fprintf(stderr, "startServer failed\n");
            return 1;
        }
        printf("Server shutting down.\n");
    } else {
        fprintf(stderr, "Invalid command\n");
        return 1;
    }

    return 0;
}
