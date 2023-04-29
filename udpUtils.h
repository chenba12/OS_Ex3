#ifndef OS_EX3_UDPUTILS_H
#define OS_EX3_UDPUTILS_H

#include <stdbool.h>
#include <unistd.h>
#include "utils.h"

void udpServer(pThreadData data, bool ipv4);

void receiveUdpFile(int sockfd, socklen_t addrlen);

void getFileUDPAndSendTime(pThreadData data, int clientfd, socklen_t addrlen);

void udpClient(pThreadData data, bool ipv4);

void sendUdpFile(int sockfd, struct sockaddr *addr, socklen_t addrlen);


#endif
