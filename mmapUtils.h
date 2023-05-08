#ifndef OS_EX3_MMAPUTILS_H
#define OS_EX3_MMAPUTILS_H

#include "utils.h"
#include "stdio.h"

void mmapServer(pThreadData data);


void mmapClient(pThreadData data);

void calculateChecksum(const void *data, size_t size, unsigned char *checksum);

#endif
