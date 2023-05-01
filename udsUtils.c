#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <sys/un.h>

#include "udsUtils.h"

/**
 *
 * @param data struct to pass data from the main thread
 * @param datagram true if its datagram false for stream
 */
void udsServer(pThreadData data, bool datagram) {
    unlink("/tmp/uds_socket");
    int type = datagram ? SOCK_DGRAM : SOCK_STREAM;
    int server_socket = socket(AF_UNIX, type, 0);
    if (server_socket == -1) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/uds_socket", sizeof(addr.sun_path) - 1);

    if (bind(server_socket, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(1);
    }

    if (!datagram) {
        if (listen(server_socket, 5) == -1) {
            perror("listen");
            exit(1);
        }
    }

    send(data->socket, "~~Ready~~!", strlen("~~Ready~~!"), 0);

    while (1) {
        if (datagram) {
            struct sockaddr_un client_addr;
            socklen_t addrLen = sizeof(client_addr);
            getFileUDSAndSendTime(data, server_socket, datagram, &client_addr, &addrLen);
        } else {
            struct sockaddr_storage client_addr;
            socklen_t addrLen = sizeof(client_addr);
            int clientFD = accept(server_socket, (struct sockaddr *) &client_addr, &addrLen);
            if (clientFD == -1) {
                perror("accept");
                continue;
            }
            getFileUDSAndSendTime(data, clientFD, datagram, NULL, NULL);

        }
        break;
    }


    char done[5];
    snprintf(done, sizeof(done), "DONE");
    send(data->socket, done, strlen(done), 0);

    sleep(2);
    close(server_socket);
    if (remove("/tmp/uds_socket") == -1) {
        perror("remove");
        exit(1);
    }
}
/**
 *
 * @param data struct to pass data from the main thread
 * @param server_fd
 * @param datagram true if its datagram false for stream
 * @param client_addr
 * @param addrLen
 */
void getFileUDSAndSendTime(pThreadData data, int server_fd, bool datagram, struct sockaddr_un *client_addr,
                           socklen_t *addrLen) {
    long startTime = getCurrentTime();
    receiveUDSFile(server_fd, datagram, client_addr, addrLen);
    long endTime = getCurrentTime();
    long elapsedTime = endTime - startTime;
    char elapsedStr[200];
    if (strcmp(data->testParam, "stream\n") == 0) {
        snprintf(elapsedStr, sizeof(elapsedStr), "%s_stream,%ld\n", data->testType, elapsedTime);
    } else {
        snprintf(elapsedStr, sizeof(elapsedStr), "%s_%s,%ld\n", data->testType, data->testParam, elapsedTime);
    }
    send(data->socket, elapsedStr, strlen(elapsedStr), 0);
}

/**
 *
 * @param server_fd
 * @param datagram
 * @param client_addr
 * @param addrLen
 */
void receiveUDSFile(int server_fd, bool datagram, struct sockaddr_un *client_addr, socklen_t *addrLen) {
    FILE *fp = fopen("received_file", "wb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    char buffer[1024] = {0};
    ssize_t bytes_read;
    size_t total_bytes_read = 0;

    if (datagram) {
        while ((bytes_read = recvfrom(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr *) client_addr, addrLen)) >
               0) {
            if (fwrite(buffer, 1, bytes_read, fp) != bytes_read) {
                perror("fwrite");
                exit(1);
            }
            total_bytes_read += bytes_read;
            if (total_bytes_read >= 100 * 1024 * 1024 || total_bytes_read == 104783872) {
                break;
            }
        }
    } else {
        while ((bytes_read = recv(server_fd, buffer, sizeof(buffer), 0)) > 0) {
            if (fwrite(buffer, 1, bytes_read, fp) != bytes_read) {
                perror("fwrite");
                exit(1);
            }
            total_bytes_read += bytes_read;
            if (total_bytes_read >= 100 * 1024 * 1024 || total_bytes_read == 104783872) {
                break;
            }
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
 * @param datagram true if its datagram false for stream
 */
void udsClient(pThreadData data, bool datagram) {
    int socket_type = datagram ? SOCK_DGRAM : SOCK_STREAM;
    int client_socket = socket(AF_UNIX, socket_type, 0);
    if (client_socket == -1) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_un serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, "/tmp/uds_socket", sizeof(serv_addr.sun_path) - 1);

    if (connect(client_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        exit(1);
    }

    sendUDSFile(client_socket, datagram);

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
 * @param datagram true if its datagram false for stream
 */
void sendUDSFile(int clientFD, bool datagram) {
    FILE *fp = fopen("file", "rb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    char buffer[1024] = {0};
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (datagram) {
            if (sendto(clientFD, buffer, bytes_read, 0, NULL, 0) == -1) {
                perror("sendto");
                exit(1);
            }
        } else {
            if (send(clientFD, buffer, bytes_read, 0) == -1) {
                perror("send");
                exit(1);
            }
        }
    }
    fclose(fp);
}