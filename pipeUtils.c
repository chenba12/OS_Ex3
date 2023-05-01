#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/socket.h>
#include "pipeUtils.h"

/**
 * pipe server method
 * here we need to send the ready message before opening the pipe or else the thread will hang and block
 * the server create a pipe named my_pipe with read | create flags
 * reads the data the client sent and write a file named file_received
 * @param data struct to pass data from the main thread
 */
void pipeServer(pThreadData data) {
    int pipe_fd;
    unsigned long bytes_read, bytes_written;
    char *filename = "file_received";
    send(data->socket, "~~Ready~~!", strlen("~~Ready~~!"), 0);
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

/**
 * open the file the user sent for reading and open my_pipe for writing
 * and send over the file over the pipe
 * @param data struct to pass data from the main thread
 */
void pipeClient(pThreadData data) {
    int pipe_fd;
    unsigned long bytes_read, bytes_written;
    char buffer[1024] = {0};

    FILE *fp = fopen(data->testParam, "rb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    pipe_fd = open("/tmp/my_pipe", O_WRONLY);
    if (pipe_fd == -1) {
        perror("open");
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