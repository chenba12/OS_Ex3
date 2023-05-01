#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include "pipeUtils.h"

void pipeServer(pThreadData data) {
    printf("hey\n");
    int pipe_fd;
    unsigned long bytes_read, bytes_written;
    char *filename = "file_received";
    char readyStr[11];
    snprintf(readyStr, sizeof(readyStr), "~~Ready~~!");
    send(data->socket, readyStr, strlen(readyStr), 0);
    // Open the pipe for reading

    pipe_fd = open("/tmp/my_pipe", O_RDONLY | O_CREAT, 0666);
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

    // Read from the pipe and write to the output file
    long startTime = getCurrentTime();
    char buffer[1024] = {0};
    while ((bytes_read = read(pipe_fd, buffer, sizeof(buffer))) > 0) {
        bytes_written = fwrite(buffer, sizeof(char), bytes_read, fp);
        if (bytes_written != bytes_read) {
            perror("fwrite");
            exit(1);
        }
    }
    if (bytes_read == -1) {
        perror("read");
        exit(1);
    }
    long endTime = getCurrentTime();
    long elapsedTime = endTime - startTime;
    char elapsedStr[200];
    snprintf(elapsedStr, sizeof(elapsedStr), "%s,%ld\n", data->testType, elapsedTime);
    printf("%s\n", elapsedStr);

    fclose(fp);
    close(pipe_fd);
}

void pipeClient(pThreadData data) {
    int pipe_fd;
    unsigned long bytes_read, bytes_written;
    char buffer[1024] = {0};

    // Create the named pipe
    mkfifo("/tmp/my_pipe", 0666);

    // Open the pipe for writing
    pipe_fd = open("/tmp/my_pipe", O_WRONLY);
    if (pipe_fd == -1) {
        perror("open");
        exit(1);
    }

    // Open the file to be sent
    FILE *fp = fopen(data->testParam, "rb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }

    // Send the file data through the named pipe
    while ((bytes_read = fread(buffer, 1, 1024, fp)) > 0) {
        bytes_written = write(pipe_fd, buffer, bytes_read);
        if (bytes_written == -1) {
            perror("write");
            exit(1);
        }
        memset(buffer, 0, 1024);
    }
    fclose(fp);
    close(pipe_fd);
}