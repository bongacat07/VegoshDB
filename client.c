
#include <stdio.h>
#include <stdint.h>
#include "netUtils.h"
#include "protocol.h"

/**
 * @brief Wire-format request structure sent to the server.
 *
 * Layout:
 *   [opcode:1][key_len:1][val_len:1][key][value]
 *
 * key and value buffers are fixed-size but only the first
 * key_len / val_len bytes are transmitted.
 */
typedef struct {
    uint8_t opcode;
    uint8_t key_len;
    uint8_t val_len;
    char key[16];
    char val[32];
} Request;

/**
 * @brief Parsed command from user input.
 *
 * Uses larger buffers since input is read as text and later
 * validated against protocol limits.
 */
typedef struct {
    uint8_t opcode;
    char key[256];
    char val[256];
} Command;

/**
 * @brief Builds a Request from a parsed Command.
 *
 * Validates key/value length against protocol limits and
 * copies the data into the fixed-size wire structure.
 *
 * If validation fails, opcode is set to 0xFF to signal error.
 */


/**
 * @brief Interactive client loop.
 *
 * Reads commands from stdin, converts them into the wire
 * protocol, sends them to the server, and prints responses.
 *
 * Supported commands:
 *   SET <key> <value>
 *   GET <key>
 */
void shellLoop(int connfd) {
    char line[1024];

    /* Read–eval–print loop. */
    while (1) {
        printf("> ");

        /* Read a full line from stdin. */
        if (fgets(line, sizeof(line), stdin) == NULL)
            break;

        /* Strip trailing newline. */
        line[strcspn(line, "\n")] = 0;

        Command cmd = {0};
        char op[16];

        /* Parse user input into op/key/value tokens. */
        int n = sscanf(line, "%15s %255s %255s", op, cmd.key, cmd.val);
        if (n < 1)
            continue;

        /* Map textual command to protocol opcode. */
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

        /* Enforce protocol size limits before sending. */
        if (key_len > 16) { fprintf(stderr, "key too long\n");   continue; }
        if (val_len > 32) { fprintf(stderr, "value too long\n"); continue; }

        /* Send wire format:
         *   opcode [key_len] [val_len] key [value]
         * Note: GET does not include val_len or value.
         */
        writen(connfd, &cmd.opcode, 1);
        writen(connfd, &key_len,    1);

        if (cmd.opcode == 0x01)
            writen(connfd, &val_len, 1);

        writen(connfd, cmd.key, key_len);

        if (cmd.opcode == 0x01)
            writen(connfd, cmd.val, val_len);

        /* Read server status byte. */
        uint8_t response;
        if (readn(connfd, &response, 1) == -1) {
            perror("readn");
            break;
        }

        /* Decode server response. */
        switch (response) {
            case SUCCESS:               printf("OK\n");                  break;
            case KEY_NOT_FOUND:         printf("ERR: key not found\n");  break;
            case KEY_EXISTS_UPDATED:    printf("OK: key updated\n");     break;
            case MAX_KEY_LIMIT_REACHED: printf("ERR: store full\n");     break;
            default:
                printf("ERR: unknown response 0x%02x\n", response);
                break;
        }

        /* For successful GET, read and print returned value. */
        if (cmd.opcode == 0x02 && response == SUCCESS) {
            uint8_t vlen;
            uint8_t val[32];

            readn(connfd, &vlen, 1);
            readn(connfd, val,   vlen);

            printf("%.*s\n", (int)vlen, val);
        }
    }
}

/**
 * @brief Connects to the server and starts the interactive shell.
 *
 * Creates a TCP connection to the provided IPv4 address on port 8080,
 * then enters shellLoop() for user interaction.
 */
int startClient(const char *ip) {
    int connfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connfd == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in servaddr;

    /* Initialize server address structure. */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8080);

    /* Convert textual IP into binary form. */
    if (inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "invalid IP address\n");
        close(connfd);
        return -1;
    }

    /* Establish TCP connection to server. */
    if (connect(connfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("connect");
        close(connfd);
        return -1;
    }

    /* Enter interactive client loop. */
    shellLoop(connfd);
    return 0;
}
