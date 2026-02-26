
#include <stdio.h>
#include <stdint.h>
#include "netUtils.h"
#include "protocol.h"

typedef struct {
    uint8_t opcode;
    uint8_t key_len;
    uint8_t val_len;
    char key[16];
    char val[32];
} Request;

typedef struct {
    uint8_t opcode;
    char key[256];
    char val[256];
} Command;

Request buildRequest(Command cmd) {
    Request req = {0};

    if (strlen(cmd.key) > 16) {
        fprintf(stderr, "key too long\n");
        req.opcode = 0xFF;
        return req;
    }
    if (strlen(cmd.val) > 32) {
        fprintf(stderr, "value too long\n");
        req.opcode = 0xFF;
        return req;
    }

    req.opcode  = cmd.opcode;
    req.key_len = strlen(cmd.key);
    req.val_len = strlen(cmd.val);
    memcpy(req.key, cmd.key, req.key_len);
    memcpy(req.val, cmd.val, req.val_len);

    return req;
}

void shellLoop(int connfd) {
    char line[1024];
    while (1) {
        printf("> ");
        if (fgets(line, sizeof(line), stdin) == NULL)
            break;
        line[strcspn(line, "\n")] = 0;

        Command cmd = {0};
        char op[16];
        int n = sscanf(line, "%15s %255s %255s", op, cmd.key, cmd.val);
        if (n < 1)
            continue;

        if (strcmp(op, "GET") == 0 && n >= 2)
            cmd.opcode = 0x02;
        else if (strcmp(op, "SET") == 0 && n >= 3)
            cmd.opcode = 0x01;
        else {
            fprintf(stderr, "Unknown command\n");
            continue;
        }

        uint8_t key_len = (uint8_t)strlen(cmd.key);
        uint8_t val_len = (uint8_t)strlen(cmd.val);

        if (key_len > 16) { fprintf(stderr, "key too long\n");   continue; }
        if (val_len > 32) { fprintf(stderr, "value too long\n"); continue; }

        /* Send wire format: opcode [key_len] [val_len] key [value] */
        writen(connfd, &cmd.opcode, 1);
        writen(connfd, &key_len,    1);
        if (cmd.opcode == 0x01)
            writen(connfd, &val_len, 1);
        writen(connfd, cmd.key, key_len);
        if (cmd.opcode == 0x01)
            writen(connfd, cmd.val, val_len);

        uint8_t response;
        if (readn(connfd, &response, 1) == -1) {
            perror("readn");
            break;
        }
        switch (response) {
            case SUCCESS:               printf("OK\n");               break;
            case KEY_NOT_FOUND:         printf("ERR: key not found\n"); break;
            case KEY_EXISTS_UPDATED:    printf("OK: key updated\n");  break;
            case MAX_KEY_LIMIT_REACHED: printf("ERR: store full\n");  break;
            default: printf("ERR: unknown response 0x%02x\n", response); break;
        }

        if (cmd.opcode == 0x02 && response == SUCCESS) {
            uint8_t vlen;
            uint8_t val[32];
            readn(connfd, &vlen, 1);
            readn(connfd, val,   vlen);
            printf("%.*s\n", (int)vlen, val);
        }
    }
}

int startClient(const char *ip) {
    int connfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connfd == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8080);

    if (inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "invalid IP address\n");
        close(connfd);
        return -1;
    }
    if (connect(connfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("connect");
        close(connfd);
        return -1;
    }

    shellLoop(connfd);
    return 0;
}
