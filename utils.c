#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <openssl/evp.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>

/**
 * get current time in milliseconds
 * @return time in milliseconds
 */
long getCurrentTime() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/**
 * change port string to number
 * @param port
 * @param ptr
 * @param host
 * @return port
 */
long getPort(long port, char **ptr, const char *host) {
    port = strtol(host, ptr, 10);
    if (port == 0) {
        perror("strtol");
        exit(2);
    }
    return port;
}

/**
 * if the program is missing flags or got wrong flags this message will be printed to the screen
 *
 */
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

/**
 * delete the received_file at server startup
 */
void deleteFile() {
    const char *filename = "received_file";
    if (access(filename, F_OK) != -1) {
        if (remove(filename) == 0) {

        } else {
            perror("remove");
        }
    }
}

/**
 * create the 100mb file at server startup
 */
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

bool verifyChecksum(const char *filePath, const unsigned char *receivedChecksum) {
    FILE *fp = fopen(filePath, "rb");
    if (fp == NULL) {
        perror("fopen");
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

    char buffer[1024] = {0};
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (EVP_DigestUpdate(mdctx, buffer, bytes_read) != 1) {
            perror("EVP_DigestUpdate");
            exit(1);
        }
    }

    unsigned int checksumLen;
    unsigned char calculatedChecksum[EVP_MAX_MD_SIZE];
    if (EVP_DigestFinal_ex(mdctx, calculatedChecksum, &checksumLen) != 1) {
        perror("EVP_DigestFinal_ex");
        exit(1);
    }

    EVP_MD_CTX_free(mdctx);
    fclose(fp);

    return memcmp(calculatedChecksum, receivedChecksum, checksumLen) == 0;
}

//void ipv4ToIpv6(const char *ipv4Str, char *ipv6Str) {
//    struct sockaddr_in ipv4Address;
//    struct sockaddr_in6 ipv6Address;
//
//    memset(&ipv4Address, 0, sizeof(struct sockaddr_in));
//    ipv4Address.sin_family = AF_INET;
//    inet_pton(AF_INET, ipv4Str, &(ipv4Address.sin_addr));
//
//    // Initialize the IPv6 address structure
//    memset(&ipv6Address, 0, sizeof(struct sockaddr_in6));
//    ipv6Address.sin6_family = AF_INET6;
//
//    // Convert the IPv4 address to an IPv4-mapped IPv6 address
//    uint32_t ipv4_part = ipv4Address.sin_addr.s_addr;
//    uint8_t ipv4_mapped_ipv6[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
//    memcpy(&ipv4_mapped_ipv6[12], &ipv4_part, sizeof(uint32_t));
//    memcpy(&ipv6Address.sin6_addr, ipv4_mapped_ipv6, sizeof(ipv4_mapped_ipv6));
//
//    inet_ntop(AF_INET6, &(ipv6Address.sin6_addr), ipv6Str, INET6_ADDRSTRLEN);
//}
