#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <sys/un.h>
#include "udsUtils.h"
#include <openssl/evp.h>
#include <errno.h>

long fileSize = 104857600;

/**
 * open a uds server socket with type sock dgram/ sock stream
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
    close(server_socket);
    if (remove("/tmp/uds_socket") == -1) {
        perror("remove");
        exit(1);
    }
}

/**
 * send the end time to the client
 * @param data struct to pass data from the main thread
 * @param server_fd server socket
 * @param datagram true if its datagram false for stream
 * @param client_addr client address info
 * @param addrLen client address length
 */
void getFileUDSAndSendTime(pThreadData data, int server_fd, bool datagram, struct sockaddr_un *client_addr,
                           socklen_t *addrLen) {
    receiveUDSFile(data, server_fd, datagram, client_addr, addrLen);
    long endTime = getCurrentTime();
    char endTimeStr[200];
    snprintf(endTimeStr, sizeof(endTimeStr), "endTime %ld\n", endTime);
    if (datagram) {
        if (sendto(server_fd, endTimeStr, sizeof(endTimeStr), 0, NULL, 0) == -1) {
            perror("sendto");
            exit(1);
        }
    } else {
        if (send(server_fd, endTimeStr, sizeof(endTimeStr), 0) == -1) {
            perror("send");
            exit(1);
        }
    }
}

/**
 * receive the 100mb file and checksum from the client
 * @param server_fd server socket
 * @param datagram true if its datagram false for stream
 * @param client_addr client address info
 * @param addrLen client address length
 */
void
receiveUDSFile(pThreadData data, int server_fd, bool datagram, struct sockaddr_un *client_addr, socklen_t *addrLen) {
    FILE *fp = fopen("file_received", "wb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    char buffer[1024] = {0};
    ssize_t bytes_read;
    size_t total_bytes_read = 0;

    if (datagram) {
        setTimeout(server_fd, maxTimeout);
        while ((bytes_read = recvfrom(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr *) client_addr, addrLen)) >
               0) {
            if (fwrite(buffer, 1, bytes_read, fp) != bytes_read) {
                perror("fwrite");
                exit(1);
            }
            total_bytes_read += bytes_read;
            if (total_bytes_read >= fileSize) {
                break;
            }
        }
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (!data->quiteMode)printf("Timeout reached\n");
            } else {
                perror("recv");
                exit(1);
            }
        }
    } else {
        setTimeout(server_fd, maxTimeout);
        while ((bytes_read = recv(server_fd, buffer, sizeof(buffer), 0)) > 0) {
            if (fwrite(buffer, 1, bytes_read, fp) != bytes_read) {
                perror("fwrite");
                exit(1);
            }
            total_bytes_read += bytes_read;
            if (total_bytes_read >= fileSize) {
                break;
            }
        }
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (!data->quiteMode)printf("Timeout reached\n");
            } else {
                perror("recv");
                exit(1);
            }
        }
    }
    fclose(fp);
    unsigned char received_checksum[32];
    if (datagram) {
        if (recvfrom(server_fd, received_checksum, sizeof(received_checksum), 0, (struct sockaddr *) client_addr,
                     addrLen) !=
            sizeof(received_checksum)) {
            perror("recv");
            exit(1);
        }
    } else {
        if (recv(server_fd, received_checksum, sizeof(received_checksum), 0) != sizeof(received_checksum)) {
            perror("recv");
            exit(1);
        }
    }
    setTimeout(server_fd, noTimeout);
    // Verify the checksum
    if (verifyChecksum("file_received", received_checksum)) {
        if (!data->quiteMode)printf("Checksum verification succeeded: The received file is intact.\n");
    } else {
        if (!data->quiteMode)printf("Checksum verification failed: The received file is corrupted or altered.\n");
    }
}

/**opens a uds client and connect to the uds server
 * uds client send a 100mb file and checksum to the server
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
    long startTime = getCurrentTime();
    sendUDSFile(client_socket, datagram);

    char buffer[64] = {0};
    ssize_t bytes_received;
    long endTime = 0;
    setTimeout(client_socket, maxTimeout);
    while ((bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        int result = sscanf(buffer, "endTime %ld", &endTime);
        if (result == 1) {
            break;
        }
    }
    if (bytes_received == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (!data->quiteMode)printf("Timeout reached\n");
            endTime = getCurrentTime();
        } else {
            perror("recv");
            exit(1);
        }
    }
    setTimeout(client_socket, noTimeout);
    long elapsedTime = endTime - startTime;
    printf("%s_%s,%ld\n", data->testType, data->testParam, elapsedTime);
    close(client_socket);
}

/**
 * this method handle the file and checksum sending
 * @param clientFD client socket
 * @param datagram true if its datagram false for stream
 */
void sendUDSFile(int clientFD, bool datagram) {
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
    unsigned int checksumLen;
    unsigned char checksum[EVP_MAX_MD_SIZE];
    if (EVP_DigestFinal_ex(mdctx, checksum, &checksumLen) != 1) {
        perror("EVP_DigestFinal_ex");
        exit(1);
    }

    if (datagram) {
        if (sendto(clientFD, checksum, checksumLen, 0, NULL, 0) == -1) {
            perror("sendto");
            exit(1);
        }
    } else {
        if (send(clientFD, checksum, checksumLen, 0) == -1) {
            perror("send");
            exit(1);
        }
    }
    EVP_MD_CTX_free(mdctx);
    fclose(fp);
}