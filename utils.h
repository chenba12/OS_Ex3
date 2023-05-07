#ifndef OS_EX3_UTILS_H
#define OS_EX3_UTILS_H

#include <stdbool.h>

/**
 * a struct used to pass data to the thread
 */
typedef struct {
    char *testParam;
    char *testType;
    int socket;
    long port;
    char *ip;
    bool quiteMode;
    char *recvBuff;
} ThreadData, *pThreadData;

long getCurrentTime();

long getPort(long port, char **ptr, const char *host);

void errorMessage();

void deleteFile();

void createFile();

bool verifyChecksum(const char *filePath, const unsigned char *receivedChecksum);

//void ipv4ToIpv6(const char *ipv4Str, char *ipv6Str);

#endif
