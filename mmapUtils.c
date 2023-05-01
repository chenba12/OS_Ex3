#include "mmapUtils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/stat.h>


void mmapServer(pThreadData data) {
    const char *shm_name = "/my_shm";
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    // Set the size of the shared memory segment
    off_t size = 100 * 1024 * 1024;
    if (ftruncate(shm_fd, size) == -1) {
        perror("ftruncate");
        exit(1);
    }

    // Map the shared memory segment into memory
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    struct stat file_stat;
    if (fstat(shm_fd, &file_stat) == -1) {
        perror("fstat");
        exit(1);
    }
    send(data->socket, "~~Ready~~!", strlen("~~Ready~~!"), 0);

    FILE *fp = fopen("file_received", "wb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    long startTime = getCurrentTime();
    if (fwrite(addr + sizeof(int), 1, size - sizeof(int), fp) != size - sizeof(int)) {
        perror("fwrite");
        exit(1);
    }
    long endTime = getCurrentTime();
    long elapsedTime = endTime - startTime;
    char elapsedStr[200];
    snprintf(elapsedStr, sizeof(elapsedStr), "%s,%ld\n", data->testType, elapsedTime);
    printf("%s\n", elapsedStr);
    fclose(fp);

    if (munmap(addr, size) == -1) {
        perror("munmap");
        exit(1);
    }

    if (shm_unlink(shm_name) == -1) {
        perror("shm_unlink");
        exit(1);
    }
}


void mmapClient(pThreadData data) {
    int fd = open(data->testParam, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        exit(1);
    }

    size_t size = sb.st_size;
    void *addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    const char *shm_name = "/my_shm";
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(shm_fd, size) == -1) {
        perror("ftruncate");
        exit(1);
    }

    void *shm_addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_addr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    memcpy(shm_addr, addr, size);

    if (munmap(shm_addr, size) == -1) {
        perror("munmap");
        exit(1);
    }

    if (close(fd) == -1) {
        perror("close");
        exit(1);
    }

    if (shm_unlink(shm_name) == -1) {
        perror("shm_unlink");
        exit(1);
    }
}