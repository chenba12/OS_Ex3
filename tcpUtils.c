#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include "tcpUtils.h"
#include "utils.h"
#include <openssl/evp.h>

/**
 * opens a tcpserver to receive the data works on both ipv4 and ipv6
 * write the data into a file named file_received
 * @param data struct to pass data from the main thread
 * @param ipv4 true to use ipv4 false to use ipv6
 */
void ipvTcpServer(pThreadData data, bool ipv4) {
    int domain = ipv4 ? AF_INET : AF_INET6;
    int server_socket = socket(domain, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        exit(1);
    }

    int val = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    if (ipv4) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(data->port);
        if (bind(server_socket, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
            perror("bind");
            exit(1);
        }
    } else {
        struct sockaddr_in6 addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_addr = in6addr_loopback;
        addr.sin6_port = htons(data->port);
        if (bind(server_socket, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
            perror("bind");
            exit(1);
        }
    }

    if (listen(server_socket, 5) == -1) {
        perror("listen");
        exit(1);
    }

    send(data->socket, "~~Ready~~!", strlen("~~Ready~~!"), 0);

    while (1) {
        struct sockaddr_storage client_addr;
        socklen_t addrLen = sizeof(client_addr);
        int clientFD = accept(server_socket, (struct sockaddr *) &client_addr, &addrLen);
        if (clientFD == -1) {
            perror("accept");
            continue;
        }
        getFileTCPAndSendTime(data, clientFD);
        close(clientFD);
        break;
    }
    close(server_socket);
}

/**
 * calculate the time,receive the file and print the results
 * @param data struct to pass data from the main thread
 * @param clientFD
 */
void getFileTCPAndSendTime(pThreadData data, int clientFD) {
    receiveTCPFile(data, clientFD);
    long endTime = getCurrentTime();
    char endTimeStr[200];
    snprintf(endTimeStr, sizeof(endTimeStr), "endTime %ld\n", endTime);
    if (send(clientFD, endTimeStr, sizeof(endTimeStr), 0) == -1) {
        perror("send");
        exit(1);
    }
}

/**
 * receive the file and write the data into a file named received_file
 * @param clientFD the client socket
 */
void receiveTCPFile(pThreadData data, int clientFD) {
    FILE *fp = fopen("file_received", "wb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    char buffer[1024] = {0};
    ssize_t bytes_read;
    ssize_t total_bytes_read = 0;
    while ((bytes_read = recv(clientFD, buffer, sizeof(buffer), 0)) > 0) {
        if (fwrite(buffer, 1, bytes_read, fp) != bytes_read) {
            perror("fwrite");
            exit(1);
        }
        total_bytes_read += bytes_read;
        if (total_bytes_read >= 104857600) {
            break;
        }
    }
    if (bytes_read == -1) {
        perror("recv");
        exit(1);
    }
    fclose(fp);
    unsigned char received_checksum[32];
    if (recv(clientFD, received_checksum, sizeof(received_checksum), 0) != sizeof(received_checksum)) {
        perror("recv");
        exit(1);
    }

    // Verify the checksum
    if (verifyChecksum("file_received", received_checksum)) {
        if (!data->quiteMode)printf("Checksum verification succeeded: The received file is intact.\n");
    } else {
        if (!data->quiteMode)printf("Checksum verification failed: The received file is corrupted or altered.\n");
    }
}

/**
 * open a tcp connection with the server works with ipv4 and ipv6
 * waits for a message from the server sayings its done with the transfer
 * @param data struct to pass data from the main thread
 * @param ipv4 true to use ipv4 false to use ipv6
 */
void ipvTcpClient(pThreadData data, bool ipv4) {
    int domain = ipv4 ? AF_INET : AF_INET6;
    int client_socket = socket(domain, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("socket");
        exit(1);
    }

    if (ipv4) {
        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(data->port);
        if (inet_pton(AF_INET, data->ip, &serv_addr.sin_addr) <= 0) {
            perror("inet_pton");
            exit(1);
        }
        if (connect(client_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
            perror("connect");
            exit(1);
        }
    } else {
        struct sockaddr_in6 serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin6_family = AF_INET6;
        serv_addr.sin6_port = htons(data->port);
        if (inet_pton(AF_INET6, "::1", &serv_addr.sin6_addr) <= 0) {
            perror("inet_pton");
            exit(1);
        }
        if (connect(client_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
            perror("connect");
            exit(1);
        }
    }
    long startTime = getCurrentTime();
    sendTCPFile(data, client_socket);
    char buffer[64] = {0};
    ssize_t bytes_received;
    long endTime = 0;
    while ((bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        int result = sscanf(buffer, "endTime %ld", &endTime);
        if (result == 1) {
            break;
        }
        if (bytes_received == -1) {
            perror("recv");
            exit(1);
        }
    }
    long elapsedTime = endTime - startTime;
    printf("%s_%s,%ld\n", data->testType, data->testParam, elapsedTime);
    close(client_socket);
}

/**
 * send the 100mb file to the server
 * @param clientFD the client socket
 */
void sendTCPFile(pThreadData data, int clientFD) {
    FILE *fp = fopen("file", "rb");
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
        if (send(clientFD, buffer, bytes_read, 0) == -1) {
            perror("send");
            exit(1);
        }
    }
    unsigned int checksumLen;
    unsigned char checksum[EVP_MAX_MD_SIZE];
    if (EVP_DigestFinal_ex(mdctx, checksum, &checksumLen) != 1) {
        perror("EVP_DigestFinal_ex");
        exit(1);
    }

    EVP_MD_CTX_free(mdctx);
    fclose(fp);

    if (send(clientFD, checksum, checksumLen, 0) == -1) {
        perror("send");
        exit(1);
    }
}