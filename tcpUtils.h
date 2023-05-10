#ifndef OS_EX3_TCPUTILS_H
#define OS_EX3_TCPUTILS_H

#include "utils.h"


void ipvTcpServer(pThreadData data, bool ipv4);

void getFileTCPAndSendTime(pThreadData data, int clientFD);

void receiveTCPFile(pThreadData data, int clientFD);

void ipvTcpClient(pThreadData data, bool ipv4);

void sendTCPFile(int clientFD);


#endif