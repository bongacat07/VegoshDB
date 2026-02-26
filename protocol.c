/**
 * @protocol.c
 * @Implementation of the protocol
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include "netUtils.h"
#include "vegosh.h"
#include "protocol.h"
/**
 * @brief Reads key_len and val_len from the socket, then reads
 * exactly that many bytes for key and value respectively.
 * Calls insert() and sends back the appropriate status byte.
 */
int handle_insert(int connfd) {
    uint8_t key_len, val_len;
    readn(connfd, &key_len, 1);
    readn(connfd, &val_len, 1);

    if (key_len > 16 || val_len > 32) {
        uint8_t response = INVALID_OPCODE;
        writen(connfd, &response, 1);
        return -1;
    }

    uint8_t key[16]   = {0};
    uint8_t value[32] = {0};
    readn(connfd, key, key_len);
    readn(connfd, value, val_len);

    int result = insert(key, value, &val_len);
    uint8_t response;
    if      (result ==  0) response = SUCCESS;
    else if (result ==  1) response = KEY_EXISTS_UPDATED;
    else if (result == -1) response = KEY_NOT_FOUND;
    else if (result == -2) response = MAX_KEY_LIMIT_REACHED;
    else                   response = INVALID_OPCODE;
    writen(connfd, &response, 1);
    return 0;
}
/**
 * @brief Reads key_len from the socket, then reads exactly that
 * many bytes for the key. Sends back a status byte, followed by
 * the value if found.
 */
int handle_get(int connfd) {
    uint8_t key_len;
    readn(connfd, &key_len, 1);

    if (key_len > 16) {
        uint8_t response = INVALID_OPCODE;
        writen(connfd, &response, 1);
        return -1;
    }

    uint8_t key[16] = {0};
    readn(connfd, key, key_len);

    uint8_t value_len = 0;
    uint8_t out_value[32];
    int result = get(key, out_value, &value_len);
    if (result == -1) {
        uint8_t response = KEY_NOT_FOUND;
        writen(connfd, &response, 1);
        return 0;
    }
    uint8_t response = SUCCESS;
    writen(connfd, &response, 1);
    writen(connfd, &value_len, 1);
    writen(connfd, out_value, value_len);
    return 0;
}
/**
 * @brief Reads the opcode byte from the socket and dispatches
 * to the appropriate handler.
 *
 * SET --> 0x01
 * GET --> 0x02
 */
int parser(int connfd) {
    uint8_t opcode;
    readn(connfd, &opcode, 1);
    switch (opcode) {
        case 0x01: return handle_insert(connfd);
        case 0x02: return handle_get(connfd);
        default:
            fprintf(stderr, "Invalid opcode: 0x%02x\n", opcode);
            return -1;
    }
}
