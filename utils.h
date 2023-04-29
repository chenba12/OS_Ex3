#ifndef OS_EX3_UTILS_H
#define OS_EX3_UTILS_H
typedef struct {
    char *testParam;
    char *testType;
    int socket;
    long port;
} ThreadData, *pThreadData;

long getCurrentTime();

long getPort(long port, char **ptr, const char *host);

void errorMessage();

#endif //OS_EX3_UTILS_H
