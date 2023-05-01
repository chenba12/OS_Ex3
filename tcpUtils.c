#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include "tcpUtils.h"

/**
 *
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
        char done[5];
        snprintf(done, sizeof(done), "DONE");
        send(data->socket, done, strlen(done), 0);
        close(clientFD);
        break;
    }
    close(server_socket);
}

/**
 *
 * @param data struct to pass data from the main thread
 * @param clientFD
 */
void getFileTCPAndSendTime(pThreadData data, bool clientFD) {
    long startTime = getCurrentTime();
    receiveTCPFile(clientFD);
    long endTime = getCurrentTime();
    long elapsedTime = endTime - startTime;
    char elapsedStr[200];
    snprintf(elapsedStr, sizeof(elapsedStr), "%s_%s,%ld\n", data->testType, data->testParam, elapsedTime);
    send(data->socket, elapsedStr, strlen(elapsedStr), 0);
}

/**
 *
 * @param clientFD
 */
void receiveTCPFile(int clientFD) {
    FILE *fp = fopen("received_file", "wb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    char buffer[1024] = {0};
    size_t bytes_read;
    size_t total_bytes_read = 0;
    while ((bytes_read = recv(clientFD, buffer, sizeof(buffer), 0)) > 0) {
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
        perror("recv");
        exit(1);
    }
    fclose(fp);
}

/**
 *
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
        if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
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

    sendTCPFile(client_socket);

    char buffer[5] = {0};
    ssize_t bytes_received;
    while ((bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        if (strcmp(buffer, "DONE!") == 0) {
            break;
        }
    }
    if (bytes_received == -1) {
        perror("recv");
        exit(1);
    }

    close(client_socket);
}

/**
 *
 * @param clientFD
 */
void sendTCPFile(int clientFD) {// Send the file
    FILE *fp = fopen("file", "rb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    char buffer[1024] = {0};
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send(clientFD, buffer, bytes_read, 0) == -1) {
            perror("send");
            exit(1);
        }
    }
    fclose(fp);
}