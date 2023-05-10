#ifndef OS_EX3_UDPUTILS_H
#define OS_EX3_UDPUTILS_H

#include <stdbool.h>
#include <unistd.h>
#include "utils.h"

void udpServer(pThreadData data, bool ipv4);

void receiveUdpFile(pThreadData data, int clientFD, struct sockaddr *client_addr, socklen_t *addrLen);

void getFileUDPAndSendTime(pThreadData data, int clientFD);

void udpClient(pThreadData data, bool ipv4);

void sendUdpFile(pThreadData data, int clientFD, struct sockaddr *addr, socklen_t addrLen);


#endif
