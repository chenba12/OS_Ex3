#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>
#include "tcpUtils.h"
#include "utils.h"
#include "udpUtils.h"
#include "udsUtils.h"
#include "mmapUtils.h"
#include "pipeUtils.h"

// function signatures
void checkFlags(int argc, char *const *argv, long port, char **ptr, char *host, char *ip, bool testMode, char *testType,
                char *testParam, bool quiteMode);

void serverHandler(long port, bool testMode, bool quiteMode);

void clientHandler(char *ip, long port, bool testMode, char *testType, char *testParam);


void
startChat(int socket, long port, bool clientOrServer, bool testMode, char *testType, char *testParam, bool quiteMode);

int checkConnection(char *testType, char *testParam);

void *serverTransfer(void *args);

void *clientTransfer(void *args);

int main(int argc, char *argv[]) {
    long port = 0;
    char *ptr;
    char *host = NULL;
    char *ip = NULL;
    bool testMode = false;
    // ipv4,ipv6,mmap,pipe,uds are in argv[5]
    char *testType = NULL;
    // udp/tcp or dgram/stream are in argv[6]
    char *testParam = NULL;
    bool quiteMode = false;

    checkFlags(argc, argv, port, &ptr, host, ip, testMode, testType, testParam, quiteMode);
    return 0;
}

void checkFlags(int argc, char *const *argv, long port, char **ptr, char *host, char *ip, bool testMode, char *testType,
                char *testParam, bool quiteMode) {
    if (argc >= 3 && argc <= 7) {
        int result = strcmp(argv[1], "-c");
        if (result == 0) {
            ip = argv[2];
            host = argv[3];
            port = getPort(port, ptr, host);
            if (argc == 7) {
                if (strcmp(argv[4], "-p") == 0) {
                    testMode = true;
                }
                if (strcmp(argv[5], "ipv4") == 0) {
                    testType = argv[5];
                } else if (strcmp(argv[5], "ipv6") == 0) {
                    testType = argv[5];
                } else if (strcmp(argv[5], "mmap") == 0) {
                    testType = argv[5];
                } else if (strcmp(argv[5], "pipe") == 0) {
                    testType = argv[5];
                } else if (strcmp(argv[5], "uds") == 0) {
                    testType = argv[5];
                }
                if (strcmp(argv[6], "tcp") == 0) {
                    testParam = argv[6];
                } else if (strcmp(argv[6], "udp") == 0) {
                    testParam = argv[6];
                } else if (strcmp(argv[6], "dgram") == 0) {
                    testParam = argv[6];
                } else if (strcmp(argv[6], "stream") == 0) {
                    testParam = argv[6];
                } else if (strcmp(argv[6], "file") == 0) {
                    testParam = argv[6];
                } else {
                    errorMessage();
                }
            }
            clientHandler(ip, port, testMode, testType, testParam);
        } else {
            result = strcmp(argv[1], "-s");
            if (result == 0) {
                host = argv[2];
                port = getPort(port, ptr, host);
                if (argc >= 4) {
                    if (strcmp(argv[3], "-p") == 0) {
                        testMode = true;
                    }
                    if (argc == 5) {
                        if (strcmp(argv[4], "-q") == 0) {
                            quiteMode = true;
                        }
                    }
                }
                serverHandler(port, testMode, quiteMode);
            } else {
                errorMessage();
            }
        }
    } else {
        errorMessage();
    }
}

void serverHandler(long port, bool testMode, bool quiteMode) {
    createFile();
    deleteFile();
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
        perror("setSockopt");
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
    if (!quiteMode)printf("Waiting for incoming TCP-connections...\n");
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
        if (!quiteMode)printf("----New client connected----\n");
        fflush(stdin);
        startChat(clientSocket, port, true, testMode, NULL, NULL, quiteMode);
        close(clientSocket);
        break;
    }
    close(serverSocket);
}

void clientHandler(char *ip, long port, bool testMode, char *testType, char *testParam) {
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
    startChat(clientSocket, port, false, testMode, testType, testParam, false);
}

void
startChat(int socket, long port, bool clientOrServer, bool testMode, char *testType, char *testParam, bool quiteMode) {
    bool firstMessage = true;
    fd_set readFDs, writeFDs;
    char sendBuffer[1024] = {0};
    char recvBuffer[1024] = {0};

    // Set the sockfd to non-blocking testMode
    int flags = fcntl(socket, F_GETFL, 0);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);

    if (clientOrServer == false && testMode == true) {

        if (testType != NULL && testParam != NULL) {
            snprintf(sendBuffer, sizeof(sendBuffer), "%s %s\n", testType, testParam);
            ssize_t bytesSent = send(socket, sendBuffer, strlen(sendBuffer), 0);
            if (bytesSent == -1) {
                perror("send");
                exit(1);
            } else {
                memset(sendBuffer, 0, sizeof(sendBuffer));
            }
        }
    }
    while (1) {
        // Set up the read and write file descriptor sets
        FD_ZERO(&readFDs);
        FD_SET(socket, &readFDs);
        FD_SET(STDIN_FILENO, &readFDs); // Add stdin to readFDs for user input
        FD_ZERO(&writeFDs);
        if (strlen(sendBuffer) > 0) {
            FD_SET(socket, &writeFDs);
        }

        // Call select() to wait for the socket or stdin to become ready for reading or writing
        if (select(socket + 1, &readFDs, &writeFDs, NULL, NULL) == -1) {
            perror("select");
            exit(1);
        }
        // Handle socket readiness for reading
        if (FD_ISSET(socket, &readFDs)) {
            ssize_t bytesRead = recv(socket, recvBuffer, sizeof(recvBuffer), 0);
            if (bytesRead == -1) {
                perror("recv");
                exit(1);
            } else if (bytesRead == 0) {
                // Connection closed by remote host
                if (!quiteMode)printf("Connection closed by remote host\n");
                exit(0);
            } else if (bytesRead > 0) {
                // Process the received data
                recvBuffer[bytesRead] = '\0';
                if (!quiteMode) {
                    printf("Received message: %s\n", recvBuffer);
                }
                if (!clientOrServer && testMode && firstMessage) {
                    if (strcmp(recvBuffer, "~~Ready~~!") == 0) {
                        pthread_t thread;
                        pThreadData data = malloc(sizeof(ThreadData));
                        memset(recvBuffer, 0, sizeof(recvBuffer));
                        data->socket = socket;
                        data->port = port + 1;
                        data->testParam = testParam;
                        data->testType = testType;
                        int rc = pthread_create(&thread, NULL, clientTransfer, data);
                        if (rc) {
                            printf("ERROR; return code from pthread_create() is %d\n", rc);
                            exit(-1);
                        }
                    }
                    firstMessage = false;
                }
                if (clientOrServer && testMode && firstMessage) {
                    char *token = strtok(recvBuffer, " ");
                    if (token != NULL) {
                        free(testType); // free previous allocation
                        testType = strdup(token);

                    }
                    token = strtok(NULL, " ");
                    if (token != NULL) {
                        free(testParam); // free previous allocation
                        testParam = strdup(token);
                        size_t len = strlen(testType);
                        if (len > 0 && !islower(testParam[len - 1]) && isascii(testParam[len - 1])) {
                            testParam[len - 1] = '\0'; // remove the last character
                        }
                    }
                    pthread_t thread;
                    pThreadData data = malloc(sizeof(ThreadData));
                    memset(recvBuffer, 0, sizeof(recvBuffer));
                    data->socket = socket;
                    data->port = port + 1;
                    data->testParam = testParam;
                    data->testType = testType;
                    int rc = pthread_create(&thread, NULL, serverTransfer, data);
                    if (rc) {
                        printf("ERROR; return code from pthread_create() is %d\n", rc);
                        exit(-1);
                    }
                    firstMessage = false;
                }
            }
        }
        // Handle socket readiness for writing
        if (FD_ISSET(socket, &writeFDs)) {
            ssize_t bytesRead = send(socket, sendBuffer, strlen(sendBuffer), 0);
            if (bytesRead == -1) {
                perror("send");
                exit(1);
            } else {
                // Clear the send buffer after sending data
                memset(sendBuffer, 0, sizeof(sendBuffer));
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readFDs)) {
            fgets(sendBuffer, sizeof(sendBuffer), stdin);
            sendBuffer[strcspn(sendBuffer, "\n")] = '\0'; // Remove trailing newline
        }
    }
}


//1: IPv4 TCP
//2: IPv4 UDP
//3: IPv6 TCP
//4: IPv6 UDP
//5: UDS datagram
//6: UDS stream
//7: Memory-mapped file
//8: Pipe
//0: Otherwise
int checkConnection(char *testType, char *testParam) {
    int result = 0;
    if (strcmp(testType, "ipv4") == 0) {
        if (strcmp(testParam, "tcp") == 0) {
            result = 1;
        } else if (strcmp(testParam, "udp") == 0) {
            result = 2;
        }
    } else if (strcmp(testType, "ipv6") == 0) {
        if (strcmp(testParam, "tcp") == 0) {
            result = 3;
        } else if (strcmp(testParam, "udp") == 0) {
            result = 4;
        }
    } else if (strcmp(testType, "uds") == 0) {
        if (strcmp(testParam, "dgram") == 0) {
            result = 5;
        } else if (strcmp(testParam, "stream\n") == 0 || strcmp(testParam, "stream") == 0) {
            result = 6;
        }
    } else if (strcmp(testType, "mmap") == 0) {
        result = 7;
    } else if (strcmp(testType, "pipe") == 0) {
        result = 8;
    }
    return result;
}


void *clientTransfer(void *args) {
    pThreadData data = (ThreadData *) args;
    int connectionType = checkConnection(data->testType, data->testParam);
    switch (connectionType) {
        case 1:
            ipvTcpClient(data, true);
            break;
        case 2:
            udpClient(data, true);
            break;
        case 3:
            ipvTcpClient(data, false);
            break;
        case 4:
            udpClient(data, false);
            break;
        case 5:
            //gram
            udsClient(data, true);
            break;
        case 6:
            //stream
            udsClient(data, false);
            break;
        case 7:
            mmapClient(data);
            break;
        case 8:
            pipeClient(data);
            break;
        default:
            printf("Invalid connection type\n");
            exit(1);
    }
    free(data);
    exit(1);
}

void *serverTransfer(void *args) {
    pThreadData data = (ThreadData *) args;
    int connectionType = checkConnection(data->testType, data->testParam);
    switch (connectionType) {
        case 1:
            ipvTcpServer(data, true);
            break;
        case 2:
            udpServer(data, true);
            break;
        case 3:
            ipvTcpServer(data, false);
            break;
        case 4:
            udpServer(data, false);
            break;
        case 5:
            udsServer(data, true);
            break;
        case 6:
            udsServer(data, false);
            break;
        case 7:
            mmapServer(data);
            break;
        case 8:
            pipeServer(data);
            break;
        default:
            printf("Invalid connection type\n");
            exit(1);
    }
    free(data);
    exit(1);
}




