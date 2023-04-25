//
// Created by chen on 4/25/23.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
//check data is working crc
// function signatures
void errorMessage();


void clientHandler(char *ip, long port);

void serverHandler(long port);

long getPort(long port, char **ptr, const char *host);

void startChat(int socket);

int main(int argc, char *argv[]) {
    char *type;
    long port = 0;
    char *ptr;
    char *host;
    char *ip;
    if (argc >= 3 && argc <= 4) {
        type = argv[1];
        int result = strcmp(type, "-c");
        if (result == 0) {
            //client
            ip = argv[2];
            if (argc != 4) {
                errorMessage();
            }
            host = argv[3];
            port = getPort(port, &ptr, host);

            clientHandler(ip, port);
        } else {
            result = strcmp(type, "-s");
            if (result == 0) {
                //server
                host = argv[2];
                port = getPort(port, &ptr, host);
                serverHandler(port);
            }
        }
    }
    return 0;
}

long getPort(long port, char **ptr, const char *host) {
    port = strtol(host, ptr, 10);
    if (port == 0) {
        perror("strtol failed");
        exit(2);
    }
    return port;
}

void serverHandler(long port) {
    printf("server port %ld\n", port);
    int serverSocket;
    //Opening a new TCP socket
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Failed to open a TCP connection");
        exit(3);
    }
    //Enabling reuse of the port
    int enableReuse = 1;
    int ret = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enableReuse, sizeof(int));
    if (ret < 0) {
        perror("setSockopt() failed with error code");
        exit(4);
    }
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddress.sin_port = htons(port);
    //bind() associates the socket with its local address 127.0.0.1
    int bindResult = bind(serverSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
    if (bindResult == -1) {
        perror("Bind failed with error code");
        close(serverSocket);
        exit(5);
    }
    //Preparing to accept new in coming requests
    int listenResult = listen(serverSocket, 10);
    if (listenResult == -1) {
        perror("Bind failed with error code");
        exit(6);
    }
    printf("Waiting for incoming TCP-connections...\n");
    struct sockaddr_in clientAddress;  //
    socklen_t clientAddressLen = sizeof(clientAddress);
    while (1) {
        memset(&clientAddress, 0, sizeof(clientAddress));
        clientAddressLen = sizeof(clientAddress);
        //Accepting a new client connection
        int clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddress, &clientAddressLen);
        if (clientSocket == -1) {
            perror("listen failed with error code");
            close(serverSocket);
            exit(6);
        }
        printf("----New client connected----\n");
        fflush(stdin);
        startChat(clientSocket);
        close(clientSocket);
    }
    close(serverSocket);
}

void startChat(int socket) {
    fd_set readfds, writefds;
    char sendbuf[1024] = {0};
    char recvbuf[1024] = {0};

    // Set the sockfd to non-blocking mode
    int flags = fcntl(socket, F_GETFL, 0);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);

    while (1) {
        // Set up the read and write file descriptor sets
        FD_ZERO(&readfds);
        FD_SET(socket, &readfds);
        FD_SET(STDIN_FILENO, &readfds); // Add stdin to readfds for user input
        FD_ZERO(&writefds);
        if (strlen(sendbuf) > 0) {
            FD_SET(socket, &writefds);
        }

        // Call select() to wait for the socket or stdin to become ready for reading or writing
        if (select(socket + 1, &readfds, &writefds, NULL, NULL) == -1) {
            perror("select");
            exit(1);
        }

        // Handle socket readiness for reading
        if (FD_ISSET(socket, &readfds)) {
            int numbytes = recv(socket, recvbuf, sizeof(recvbuf), 0);
            if (numbytes == -1) {
                perror("recv");
                exit(1);
            } else if (numbytes == 0) {
                // Connection closed by remote host
                printf("Connection closed by remote host\n");
                exit(0);
            } else if (numbytes > 0) {
                // Process the received data
                recvbuf[numbytes] = '\0';
                printf("Received message: %s\n", recvbuf);
            }
        }

        // Handle socket readiness for writing
        if (FD_ISSET(socket, &writefds)) {
            int numbytes = send(socket, sendbuf, strlen(sendbuf), 0);
            if (numbytes == -1) {
                perror("send");
                exit(1);
            } else {
                // Clear the send buffer after sending data
                memset(sendbuf, 0, sizeof(sendbuf));
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            fgets(sendbuf, sizeof(sendbuf), stdin);
            sendbuf[strcspn(sendbuf, "\n")] = '\0'; // Remove trailing newline
        }
    }
}


void clientHandler(char *ip, long port) {
    printf("server port %ld\n", port);
    printf("server ip %s\n", ip);
    int clientSocket;
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Failed to open a TCP connection");
        exit(3);
    }
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    int binaryAddress = inet_pton(AF_INET, (const char *) ip, &serverAddress.sin_addr);
    if (binaryAddress <= 0) {
        perror("Failed to convert from text to binary");
        exit(4);
    }
    // Connecting to the receiver's socket
    int connection = connect(clientSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
    if (connection == -1) {
        perror("Connection error\n");
        close(clientSocket);
        exit(5);
    }
    startChat(clientSocket);
}


void errorMessage() {
    printf("Error: not enough arguments\n");
    printf("Usage: ./stnc -c <IP> <PORT>\n");
    printf("Usage: ./stnc -s <PORT>\n");
    printf("-c to run the program as client\n");
    printf("-s to run the program as server\n");
    printf("<IP> the ip of the server\n");
    printf("<PORT> the port of the server\n");
    exit(1);
}