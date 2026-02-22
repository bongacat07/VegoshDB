#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include "netUtils.h"
#include "kv.h"



int handle_insert(int connfd){
    uint8_t key[16];
    uint8_t value[32];
    uint8_t value_type = 7; //hardcoded for now
    readn(connfd, key, 16);
    readn(connfd, value, 32);
    insert(key, value, value_type);
    uint8_t response = 69;
    writen(connfd, &response, 1);
    return 0;


}

int handle_get(int connfd){
    uint8_t key[16];
    readn(connfd, key, 16);
    uint8_t out_value[32];
    uint8_t out_value_type;
    get(key, out_value, &out_value_type);
    writen(connfd, out_value, 32);
    writen(connfd, &out_value_type, 1);
    return 0;


}


int parser(int connfd){
    uint8_t methodbuf[1];
    memset(methodbuf,0,1);
    readn(connfd,methodbuf,1);

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
