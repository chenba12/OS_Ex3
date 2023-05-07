#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "udpUtils.h"
#include "utils.h"
#include <openssl/evp.h>

/**
 * opens a udp server to receive the data works on both ipv4 and ipv6
 * write the data into a file named file_received
 * @param data struct to pass data from the main thread
 * @param ipv4 true to use ipv4 false to use ipv6
 */
void udpServer(pThreadData data, bool ipv4) {
    int serverSocket;
    if (ipv4) {
        serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    } else {
        serverSocket = socket(AF_INET6, SOCK_DGRAM, 0);
    }

    if (serverSocket == -1) {
        perror("socket");
        exit(1);
    }

    int val = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    struct sockaddr_storage server_addr;
    socklen_t addrLen;
    if (ipv4) {
        struct sockaddr_in *s = (struct sockaddr_in *) &server_addr;
        memset(s, 0, sizeof(*s));
        s->sin_family = AF_INET;
        s->sin_addr.s_addr = htonl(INADDR_ANY);
        s->sin_port = htons(data->port);
        addrLen = sizeof(*s);
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *) &server_addr;
        memset(s, 0, sizeof(*s));
        s->sin6_family = AF_INET6;
        s->sin6_addr = in6addr_any;
        s->sin6_port = htons(data->port);
        addrLen = sizeof(*s);
    }

    if (bind(serverSocket, (struct sockaddr *) &server_addr, addrLen) == -1) {
        perror("bind");
        exit(1);
    }

    send(data->socket, "~~Ready~~!", strlen("~~Ready~~!"), 0);

    getFileUDPAndSendTime(data, serverSocket);
}

/**
 * calculate the time,receive the file and print the results
 * @param data struct to pass data from the main thread
 * @param clientFD client socket
 */
void getFileUDPAndSendTime(pThreadData data, int clientFD) {
    struct sockaddr_storage client_addr;
    socklen_t addrLen = sizeof(client_addr);
    receiveUdpFile(data, clientFD, (struct sockaddr *) &client_addr, &addrLen);
    long endTime = getCurrentTime();
    char endTimeStr[200];
    snprintf(endTimeStr, sizeof(endTimeStr), "endTime %ld\n", endTime);
    if (sendto(clientFD, endTimeStr, strlen(endTimeStr), 0, (struct sockaddr *) &client_addr, addrLen) == -1) {
        perror("sendto");
        exit(1);
    }
}

/**
 * receive the file and write the data into a file named received_file
 * @param clientFD client socket
 * @param client_addr client address
 * @param addrLen length of client address
 */
void receiveUdpFile(pThreadData data, int clientFD, struct sockaddr *client_addr, socklen_t *addrLen) {
    FILE *fp = fopen("file_received", "wb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    char buffer[50000] = {0};
    size_t bytes_read;
    size_t total_bytes_read = 0;
    int ack = 1;
    while ((bytes_read = recvfrom(clientFD, buffer, sizeof(buffer), 0, client_addr,
                                  addrLen)) > 0) {
        if (fwrite(buffer, 1, bytes_read, fp) != bytes_read) {
            perror("fwrite");
            exit(1);
        }
        total_bytes_read += bytes_read;

        // Send ACK
        if (sendto(clientFD, &ack, sizeof(ack), 0, client_addr, *addrLen) == -1) {
            perror("sendto");
            exit(1);
        }

        if (total_bytes_read >= 104857600) {
            break;
        }
        if (bytes_read == -1) {
            perror("recvfrom");
            exit(1);
        }
    }
    unsigned char received_checksum[32];
    if (recvfrom(clientFD, received_checksum, sizeof(received_checksum), 0, client_addr,
                 addrLen) != sizeof(received_checksum)) {
        perror("recv");
        exit(1);
    }

    // Verify the checksum
    if (verifyChecksum("file_received", received_checksum)) {
        if (!data->quiteMode)printf("Checksum verification succeeded: The received file is intact.\n");
    } else {
        if (!data->quiteMode)printf("Checksum verification failed: The received file is corrupted or altered.\n");
    }
    fclose(fp);
}


/**
 * open a tcp connection with the server works with ipv4 and ipv6
 * waits for a message from the server sayings its done with the transfer
 * @param data struct to pass data from the main thread
 * @param ipv4 true to use ipv4 false to use ipv6
 */
void udpClient(pThreadData data, bool ipv4) {
    struct sockaddr_in6 server_addr;
    struct sockaddr_in server_addr_ipv4;
    struct sockaddr *addr;
    socklen_t addrLen;
    int client_socket;
    if (ipv4) {
        client_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (client_socket == -1) {
            perror("socket");
            exit(1);
        }

        memset(&server_addr_ipv4, 0, sizeof(server_addr_ipv4));
        server_addr_ipv4.sin_family = AF_INET;
        server_addr_ipv4.sin_port = htons(data->port);

        if (inet_pton(AF_INET, data->ip, &server_addr_ipv4.sin_addr) <= 0) {
            perror("inet_pton");
            exit(1);
        }

        addr = (struct sockaddr *) &server_addr_ipv4;
        addrLen = sizeof(server_addr_ipv4);
    } else {
        client_socket = socket(AF_INET6, SOCK_DGRAM, 0);
        if (client_socket == -1) {
            perror("socket");
            exit(1);
        }

        // Fill in the server's address and port number
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin6_family = AF_INET6;
        server_addr.sin6_port = htons(data->port);

        // Convert IPv4 and IPv6 addresses from text to binary form
        if (inet_pton(AF_INET6, "::1", &server_addr.sin6_addr) <= 0) {
            perror("inet_pton");
            exit(1);
        }

        addr = (struct sockaddr *) &server_addr;
        addrLen = sizeof(server_addr);
    }
    long startTime = getCurrentTime();
    sendUdpFile(client_socket, addr, addrLen);
    char buffer[64] = {0};
    ssize_t bytes_received;
    long endTime = 0;
    struct sockaddr_storage server_addr_from;
    socklen_t addrLenFrom = sizeof(server_addr_from);
    while ((bytes_received = recvfrom(client_socket, buffer, sizeof(buffer) - 1, 0,
                                      (struct sockaddr *) &server_addr_from, &addrLenFrom)) > 0) {
        buffer[bytes_received] = '\0';
        int result = sscanf(buffer, "endTime %ld", &endTime);
        if (result == 1) {
            break;
        }
        if (bytes_received == -1) {
            perror("recvfrom");
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
 * @param addr sockaddr struct
 * @param addrLen address length
 */
void sendUdpFile(int clientFD, struct sockaddr *addr, socklen_t addrLen) {
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
    char buffer[50000];
    size_t bytes_read;
    size_t bytes_sent;
    size_t sum = 0;
    int ack;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (EVP_DigestUpdate(mdctx, buffer, bytes_read) != 1) {
            perror("EVP_DigestUpdate");
            exit(1);
        }
        if ((bytes_sent = sendto(clientFD, buffer, bytes_read, 0, addr, addrLen)) == -1) {
            perror("sendto");
            exit(1);
        }
        sum += bytes_sent;
        if (recvfrom(clientFD, &ack, sizeof(ack), 0, addr, &addrLen) == -1) {
            perror("recvfrom");
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

    if (sendto(clientFD, checksum, sizeof(checksum), 0, addr, addrLen) == -1) {
        perror("sendto");
        exit(1);
    }
    fclose(fp);
}