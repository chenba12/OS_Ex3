#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "udpUtils.h"
#include "utils.h"

/**
 *
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

    char done[5];
    snprintf(done, sizeof(done), "DONE");
    send(data->socket, done, strlen(done), 0);
    close(serverSocket);
}

/**
 *
 * @param data struct to pass data from the main thread
 * @param clientFD
 */
void getFileUDPAndSendTime(pThreadData data, int clientFD) {
    long startTime = getCurrentTime();
    receiveUdpFile(clientFD);
    long endTime = getCurrentTime();
    long elapsedTime = endTime - startTime;
    char elapsedStr[200];
    snprintf(elapsedStr, sizeof(elapsedStr), "%s_%s,%ld\n", data->testType, data->testParam, elapsedTime);
    send(data->socket, elapsedStr, strlen(elapsedStr), 0);
}

/**
 *
 * @param socketFD
 */
void receiveUdpFile(int socketFD) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    FILE *fp = fopen("received_file", "wb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    char buffer[10000] = {0};
    size_t bytes_read;
    size_t total_bytes_read = 0;
    while ((bytes_read = recvfrom(socketFD, buffer, sizeof(buffer), 0, (struct sockaddr *) &client_addr,
                                  &client_addr_len)) > 0) {
        if (fwrite(buffer, 1, bytes_read, fp) != bytes_read) {
            perror("fwrite");
            exit(1);
        }
        total_bytes_read += bytes_read;
        if (total_bytes_read >= 100 * 1024 * 1024 || total_bytes_read == 104783872) {
            break;
        }
    }
    if (bytes_read == -1) {
        perror("recvfrom");
        exit(1);
    }
    fclose(fp);
}

/**
 *
 * @param data struct to pass data from the main thread
 * @param ipv4 true to use ipv4 false to use ipv6
 */
void udpClient(pThreadData data, bool ipv4) {
    struct sockaddr_in6 server_addr;
    struct sockaddr_in server_addr_ipv4;
    struct sockaddr *addr;
    socklen_t addrLen;

    // Create a socket for the client
    int client_socket;
    if (ipv4) {
        client_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (client_socket == -1) {
            perror("socket");
            exit(1);
        }

        // Fill in the server's address and port number
        memset(&server_addr_ipv4, 0, sizeof(server_addr_ipv4));
        server_addr_ipv4.sin_family = AF_INET;
        server_addr_ipv4.sin_port = htons(data->port);

        // Convert IPv4 and IPv6 addresses from text to binary form
        if (inet_pton(AF_INET, "127.0.0.1", &server_addr_ipv4.sin_addr) <= 0) {
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

    // Send the file
    sendUdpFile(client_socket, addr, addrLen);
    char buffer[5] = {0};
    ssize_t bytes_received;
    while ((bytes_received = recv(data->socket, buffer, sizeof(buffer) - 1, 0)) >= -1) {
        buffer[bytes_received] = '\0';
        if (strcmp(buffer, "DONE!") == 0) {
            break;
        }
    }
    if (bytes_received == -1) {
        perror("recv");
        exit(1);
    }
    // Close the client socket
//    free(data);
    close(client_socket);
}

/**
 *
 * @param socketFD
 * @param addr
 * @param addrLen
 */
void sendUdpFile(int socketFD, struct sockaddr *addr, socklen_t addrLen) {
    // Send the file
    FILE *fp = fopen("file", "rb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    char buffer[10000];
    size_t bytes_read;
    size_t bytes_sent;
    size_t sum = 0;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if ((bytes_sent = sendto(socketFD, buffer, bytes_read, 0, addr, addrLen)) == -1) {
            perror("sendto");
            exit(1);
        }
        sum += bytes_sent;
        //fail transfer fails if sent too much at once
        usleep(10);
    }
    fclose(fp);
}