#ifndef OS_EX3_UDSUTILS_H
#define OS_EX3_UDSUTILS_H

#include <stdbool.h>
#include "utils.h"

void udsServer(pThreadData data, bool datagram);

void getFileUDSAndSendTime(pThreadData data, int server_fd, bool datagram, struct sockaddr_un *client_addr,
                           socklen_t *addrLen);

void receiveUDSFile(int server_fd, bool datagram, struct sockaddr_un *client_addr, socklen_t *addrLen);


void udsClient(pThreadData data, bool datagram);

void sendUDSFile(int clientFD, bool datagram);

#endif
