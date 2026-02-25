/**
 * @protocol.c
 * @Implementation of the protocol
 *
 *
 */





#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include "netUtils.h"
#include "vegosh.h"

/**
 * @brief Working of handle_insert
 *
 * Reads 16 bytes from the socket buffer into @p key.
 * Reads 32 bytes from the socket buffer into @p value.
 * Reads 1 byte from the socket buffer into @p value_len.
 *
 * After the swap, @p temp holds the old slot contents and vegosh[index]
 * holds what @p temp previously held. The status byte of the newly written
 * slot is explicitly set to OCCUPIED so callers need not worry about it.
 *
 * @param index  Index of the slot to swap with.
 * @param temp Temporary slot buffer used as the exchange partner.
 */

int handle_insert(int connfd){
    uint8_t key[16];
    uint8_t value[32];
    uint8_t value_len[1];

    readn(connfd, key, 16);
    readn(connfd, value, 32);
    readn(connfd, value_len, 1);

    int result = insert(key, value, value_len);
    if(result == -1){

    }else if (result == 0) {
        uint8_t response = 69;
        writen(connfd, &response, 1);
    }


    return 0;


}

int handle_get(int connfd) {
    uint8_t key[16];
    uint8_t value_len;
    uint8_t out_value[32];

    readn(connfd, key, 16);

    int ret = get(key, out_value, &value_len);
    if (ret != 0) {
        /* send error/not-found response to client */
        return ret;
    }

    writen(connfd, out_value, value_len);
    return 0;
}

/* -------------------------------------------------------------------------
 * Protocol parser
 * ---------------------------------------------------------------------- */

/**
 *  Read the first byte of the socket buffer
 *
 * SET --> 0X01
 * GET --> 0X02
 *
 * Call the appropriate handler
 */
int parser(int connfd){
    uint8_t methodbuf[1]; //methodbuffer
    memset(methodbuf,0,1); //set 0
    readn(connfd,methodbuf,1); //read one byte

    switch (methodbuf[0]) {
        case 0x01:{
           return handle_insert(connfd);//SET
        }
        case 0x02:{
           return handle_get(connfd);//GET
        }
        default:{
            fprintf(stderr, "Invalid method\n");
            return -1;
        }
    }

}
