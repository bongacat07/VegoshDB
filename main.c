#include "server.h"
#include "kv.h"
#include <stdio.h>

int main() {
    printf("Starting KV...\n");
    fflush(stdout);
    if (initializeKV() == -1) {
        fprintf(stderr, "initializeKV failed\n");
        return 1;
    }
    printf("Starting server...\n");
    fflush(stdout);
    if (startServer() == -1) {
        fprintf(stderr, "startServer failed\n");
        return 1;
    }
    return 0;
}