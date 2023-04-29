//
// Created by chen on 4/29/23.
//

#ifndef OS_EX3_TCPUTILS_H
#define OS_EX3_TCPUTILS_H

#include "utils.h"


void ipvTcpServer(pThreadData data, int use_ipv4);

void getFileTCPAndSendTime(pThreadData data, int clientFD);

void receiveTCPFile(int clientFD);

void ipvTcpClient(pThreadData data, int use_ipv4);

void sendTCPFile(int clientFD);


#endif //OS_EX3_TCPUTILS_H