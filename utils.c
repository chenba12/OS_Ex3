#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

long getCurrentTime() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

long getPort(long port, char **ptr, const char *host) {
    port = strtol(host, ptr, 10);
    if (port == 0) {
        perror("strtol failed");
        exit(2);
    }
    return port;
}


void errorMessage() {
    printf("Error: not enough arguments\n");
    printf("The client side: stnc -c IP PORT -p <type> <param> \n");
    printf("<IP> the ip of the server\n");
    printf("<PORT> the port of the server\n");
    printf("-c to run the program as client\n");
    printf("-p will indicate to perform the test\n"
           "<type> will be the communication types: so it can be ipv4,ipv6,mmap,pipe,uds\n"
           "<param> will be a parameter for the type. It can be udp/tcp or dgram/stream or file name:\n");
    printf("The server side: stnc -s port -p (p for performance test) -q (q for quiet)\n");
    printf("-s to run the program as server\n");
    printf("-p flag will let you know that we are going to test the performance.\n"
           "-q flag will enable quiet mode, in which only testing results will be printed.\n");
    printf("-p <type> <param> -p -q are optionals");
    exit(1);
}

void deleteFile() {
    const char *filename = "received_file";
    if (access(filename, F_OK) != -1) {
        if (remove(filename) == 0) {

        } else {
            perror("remove");
        }
    }
}

void createFile() {
    char *filename = "file";
    int fd = open(filename, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    off_t size = 100 * 1024 * 1024;
    if (ftruncate(fd, size) == -1) {
        perror("ftruncate");
        exit(1);
    }

    close(fd);
}