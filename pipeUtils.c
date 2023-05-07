#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <openssl/evp.h>
#include "pipeUtils.h"

/**
 * pipe server method
 * here we need to send the ready message before opening the pipe or else the thread will hang and block
 * the server create a pipe named my_pipe with read | create flags
 * reads the data the client sent and write a file named file_received
 * @param data struct to pass data from the main thread
 */
void pipeServer(pThreadData data) {
    unsigned char received_checksum[32];
    int pipe_fd, pipe_fd2;
    unsigned long bytes_read, bytes_written;
    char *filename = "file_received";
    send(data->socket, "~~Ready~~!", strlen("~~Ready~~!"), 0);

    pipe_fd = open("/tmp/my_pipe", O_RDONLY | O_CREAT, 0666);
    pipe_fd2 = open("/tmp/my_pipe2", O_WRONLY | O_CREAT, 0666);
    if (pipe_fd == -1) {
        perror("open");
        exit(1);
    }
    // Open the output file
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }

    char buffer[1024] = {0};
    unsigned long total_bytes_read = 0;
    unsigned long file_size = 100 * 1024 * 1024; // 100 MB
    unsigned long checksum_size = 32;

    while (total_bytes_read < file_size + checksum_size) {
        bytes_read = read(pipe_fd, buffer, sizeof(buffer));
        if (bytes_read == -1) {
            perror("read");
            exit(1);
        }

        if (total_bytes_read < file_size) {
            size_t bytes_to_write = (total_bytes_read + bytes_read > file_size) ? file_size - total_bytes_read
                                                                                : bytes_read;
            bytes_written = fwrite(buffer, sizeof(char), bytes_to_write, fp);
            if (bytes_written != bytes_to_write) {
                perror("fwrite");
                exit(1);
            }
        } else {
            size_t checksum_bytes_read = bytes_read - (file_size - total_bytes_read);
            memcpy(received_checksum + (total_bytes_read - file_size), buffer, checksum_bytes_read);
        }

        total_bytes_read += bytes_read;
    }

    fclose(fp);
    // Verify the checksum
    if (verifyChecksum("file_received", received_checksum)) {
        if (!data->quiteMode)printf("Checksum verification succeeded: The received file is intact.\n");
    } else {
        if (!data->quiteMode)printf("Checksum verification failed: The received file is corrupted or altered.\n");
    }
    sleep(2);
    close(pipe_fd);
    close(pipe_fd2);
}

/**
 * open the file the user sent for reading and open my_pipe for writing
 * and send over the file over the pipe
 * @param data struct to pass data from the main thread
 */
void pipeClient(pThreadData data) {
    int pipe_fd, pipe_fd2;
    unsigned long bytes_read, bytes_written;
    char buffer[1024] = {0};

    FILE *fp = fopen(data->testParam, "rb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    pipe_fd = open("/tmp/my_pipe", O_WRONLY);
    pipe_fd2 = open("/tmp/my_pipe2", O_RDONLY);
    if (pipe_fd == -1) {
        perror("open");
        exit(1);
    }
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (mdctx == NULL) {
        perror("EVP_MD_CTX_new");
        exit(1);
    }
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
        perror("EVP_DigestInit_ex");
        exit(1);
    }

    // Send the file data through the named pipe
    long startTime = getCurrentTime();
    while ((bytes_read = fread(buffer, 1, 1024, fp)) > 0) {
        if (EVP_DigestUpdate(mdctx, buffer, bytes_read) != 1) {
            perror("EVP_DigestUpdate");
            exit(1);
        }
        bytes_written = write(pipe_fd, buffer, bytes_read);
        if (bytes_written == -1) {
            perror("write");
            exit(1);
        }
        memset(buffer, 0, 1024);
    }
    unsigned int checksumLen;
    unsigned char checksum[EVP_MAX_MD_SIZE];
    if (EVP_DigestFinal_ex(mdctx, checksum, &checksumLen) != 1) {
        perror("EVP_DigestFinal_ex");
        exit(1);
    }

    EVP_MD_CTX_free(mdctx);
    fclose(fp);
    bytes_written = write(pipe_fd, checksum, checksumLen);
    if (bytes_written == -1) {
        perror("write");
        exit(1);
    }
    long endTime = getCurrentTime();
    long elapsedTime = endTime - startTime;
    printf("%s_%s,%ld\n", data->testType, data->testParam, elapsedTime);
    sleep(2);
    close(pipe_fd);
    close(pipe_fd2);
}