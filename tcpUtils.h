#ifndef OS_EX3_TCPUTILS_H
#define OS_EX3_TCPUTILS_H

#include "utils.h"


void ipvTcpServer(pThreadData data, bool use_ipv4);

void getFileTCPAndSendTime(pThreadData data, bool clientFD);

void receiveTCPFile(int clientFD);

void ipvTcpClient(pThreadData data, int use_ipv4);

void sendTCPFile(int clientFD);


#endif