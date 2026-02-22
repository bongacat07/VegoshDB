#ifndef PROTOCOL_H
#define PROTOCOL_H

int handle_insert(int connfd);
int handle_get(int connfd);

int parser(int connfd);

#endif
