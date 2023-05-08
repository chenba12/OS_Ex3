#include "mmapUtils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/stat.h>
#include <openssl/evp.h>

/**
 * this is the mmap server method
 * opens a mmap called my_shm(100MB) and then receive the data
 * from the client and writes it to a file called received_file
 * @param data struct passed from the main thread
 */
void mmapServer(pThreadData data) {
    FILE *fp = fopen("file_received", "wb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    const char *end_time_shm_name = "/my_shm_end_time";
    int end_time_shm_fd = shm_open(end_time_shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (end_time_shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }
    const char *semaphore_shm_name = "/my_semaphore";
    int semaphore_shm_fd = shm_open(semaphore_shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (semaphore_shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(semaphore_shm_fd, sizeof(int)) == -1) {
        perror("ftruncate");
        exit(1);
    }

    int *semaphore_addr = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, semaphore_shm_fd, 0);
    if (semaphore_addr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    *semaphore_addr = 0;
    const char *shm_name = "/my_shm";
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    off_t size = 104857600;
    if (ftruncate(shm_fd, size + EVP_MD_size(EVP_sha256())) == -1) {
        perror("ftruncate");
        exit(1);
    }

    void *addr = mmap(NULL, size + EVP_MD_size(EVP_sha256()), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    send(data->socket, "~~Ready~~!", strlen("~~Ready~~!"), 0);

    const char *checksum_shm_name = "/my_checksum_shm";
    int checksum_shm_fd = shm_open(checksum_shm_name, O_RDONLY, S_IRUSR);
    if (checksum_shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    void *checksum_addr = mmap(NULL, EVP_MAX_MD_SIZE, PROT_READ, MAP_SHARED, checksum_shm_fd, 0);
    if (checksum_addr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    unsigned char received_checksum[EVP_MAX_MD_SIZE];
    memcpy(received_checksum, checksum_addr, EVP_MD_size(EVP_sha256()));
    if (fwrite(addr, size, 1, fp) != 1) {
        perror("fwrite");
        exit(1);
    }
    fclose(fp);

    if (verifyChecksum("file_received", received_checksum)) {
        if (!data->quiteMode)printf("Checksum verification succeeded: The received file is intact.\n");
    } else {
        if (!data->quiteMode)printf("Checksum verification failed: The received file is corrupted or altered.\n");
    }
    if (munmap(checksum_addr, EVP_MAX_MD_SIZE) == -1) {
        perror("munmap");
        exit(1);
    }

    if (munmap(addr, size + EVP_MD_size(EVP_sha256())) == -1) {
        perror("munmap");
        exit(1);
    }

    long endTime = getCurrentTime();
    if (ftruncate(end_time_shm_fd, sizeof(endTime)) == -1) {
        perror("ftruncate");
        exit(1);
    }

    void *end_time_addr = mmap(NULL, sizeof(endTime), PROT_READ | PROT_WRITE, MAP_SHARED, end_time_shm_fd, 0);
    if (end_time_addr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    memcpy(end_time_addr, &endTime, sizeof(endTime));
    *semaphore_addr = 1;

    if (munmap(addr, size) == -1) {
        perror("munmap");
        exit(1);
    }
    if (munmap(semaphore_addr, sizeof(int)) == -1) {
        perror("munmap");
        exit(1);
    }

    if (shm_unlink(semaphore_shm_name) == -1) {
        perror("shm_unlink");
        exit(1);
    }
    sleep(2);

}


/**
 * open the file the user entered (data->testParam)
 * and send its data over the mmap
 * @param data struct passed from the main thread
 */
void mmapClient(pThreadData data) {
    const char *semaphore_shm_name = "/my_semaphore";
    int semaphore_shm_fd = shm_open(semaphore_shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (semaphore_shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(semaphore_shm_fd, sizeof(int)) == -1) {
        perror("ftruncate");
        exit(1);
    }

    int *semaphore_addr = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, semaphore_shm_fd, 0);
    if (semaphore_addr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    long startTime = getCurrentTime();
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

    const char *checksum_shm_name = "/my_checksum_shm";
    int checksum_shm_fd = shm_open(checksum_shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (checksum_shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(checksum_shm_fd, EVP_MAX_MD_SIZE) == -1) {
        perror("ftruncate");
        exit(1);
    }

    void *checksum_addr = mmap(NULL, EVP_MAX_MD_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, checksum_shm_fd, 0);
    if (checksum_addr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    unsigned char checksum[EVP_MAX_MD_SIZE];
    calculateChecksum(addr, size, checksum);
    memcpy(checksum_addr, checksum, EVP_MD_size(EVP_sha256()));

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

    if (munmap(addr, size) == -1) {
        perror("munmap");
        exit(1);
    }

    const char *end_time_shm_name = "/my_shm_end_time";
    int end_time_shm_fd = shm_open(end_time_shm_name, O_RDONLY, S_IRUSR);
    if (end_time_shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    while (*semaphore_addr == 0) {
        usleep(1000); // Sleep for 1000 microseconds (1 millisecond) to avoid busy-waiting
    }
    long endTime;
    void *end_time_addr = mmap(NULL, sizeof(endTime), PROT_READ, MAP_SHARED, end_time_shm_fd, 0);
    if (end_time_addr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    memcpy(&endTime, end_time_addr, sizeof(endTime));
    long elapsedTime = endTime - startTime;
    printf("%s,%ld\n", data->testType, elapsedTime);

    if (munmap(end_time_addr, sizeof(endTime)) == -1) {
        perror("munmap");
        exit(1);
    }

    if (shm_unlink(end_time_shm_name) == -1) {
        perror("shm_unlink");
        exit(1);
    }
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
    if (munmap(semaphore_addr, sizeof(int)) == -1) {
        perror("munmap");
        exit(1);
    }

    if (shm_unlink(semaphore_shm_name) == -1) {
//        perror("shm_unlink");
        exit(1);
    }

}

void calculateChecksum(const void *data, size_t size, unsigned char *checksum) {
    const EVP_MD *md = EVP_sha256();
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();

    if (mdctx == NULL) {
        fprintf(stderr, "Error: Failed to create message digest context\n");
        exit(1);
    }

    if (EVP_DigestInit_ex(mdctx, md, NULL) != 1) {
        fprintf(stderr, "Error: Failed to initialize message digest context\n");
        exit(1);
    }

    if (EVP_DigestUpdate(mdctx, data, size) != 1) {
        fprintf(stderr, "Error: Failed to update message digest context\n");
        exit(1);
    }

    unsigned int checksum_len;
    if (EVP_DigestFinal_ex(mdctx, checksum, &checksum_len) != 1) {
        fprintf(stderr, "Error: Failed to finalize message digest context\n");
        exit(1);
    }

    EVP_MD_CTX_free(mdctx);
}







