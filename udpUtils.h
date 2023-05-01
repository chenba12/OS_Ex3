#ifndef OS_EX3_UDPUTILS_H
#define OS_EX3_UDPUTILS_H

#include <stdbool.h>
#include <unistd.h>
#include "utils.h"

void udpServer(pThreadData data, bool ipv4);

void receiveUdpFile(int clientFD);

void getFileUDPAndSendTime(pThreadData data, int clientFD);

void udpClient(pThreadData data, bool ipv4);

void sendUdpFile(int clientFD, struct sockaddr *addr, socklen_t addrLen);


#endif
