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
void checkFlags(int argc, char *const *argv);

void serverHandler(long port, bool testMode, bool quiteMode, char *ip, bool partA);

void clientHandler(char *ip, long port, bool testMode, char *testType, char *testParam, bool partA);


void
startChat(int socket, long port, bool clientOrServer, bool testMode, char *testType, char *testParam, bool quiteMode,
          char *ip, bool partA);

int checkConnection(char *testType, char *testParam);

void *serverTransfer(void *args);

void *clientTransfer(void *args);

int main(int argc, char *argv[]) {


    checkFlags(argc, argv);
    return 0;
}

/**
 * check which flags are active
 * @param argc number of flags
 * @param argv array containing the flags
 */
void checkFlags(int argc, char *const *argv) {
    long port = 0;
    char *ptr;
    char *host = NULL;
    char *ip = NULL;
    bool testMode = false;
    char *testType = NULL;
    char *testParam = NULL;
    bool quiteMode = false;
    bool partA = false;
    if (argc >= 3 && argc <= 7) {
        int result = strcmp(argv[1], "-c");
        if (result == 0) {
            ip = argv[2];
            host = argv[3];
            port = getPort(port, &ptr, host);
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
            if (argc <= 5) partA = true;
            clientHandler(ip, port, testMode, testType, testParam, partA);
        } else {
            result = strcmp(argv[1], "-s");
            if (result == 0) {
                host = argv[2];
                port = getPort(port, &ptr, host);
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
                if (argc <= 5) partA = true;
                serverHandler(port, testMode, quiteMode, NULL, partA);
            } else {
                errorMessage();
            }
        }
    } else {
        errorMessage();
    }
}

/**
 * Start up a tcp server socket and launch the chat
 * @param port number
 * @param testMode if -p flag is active or not
 * @param quiteMode if -q flag is active or not
 */
void serverHandler(long port, bool testMode, bool quiteMode, char *ip, bool partA) {
    createFile();
    deleteFile();
    int serverSocket;
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Failed to open a TCP connection");
        exit(3);
    }
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
    int bindResult = bind(serverSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
    if (bindResult == -1) {
        perror("Bind failed with error code");
        close(serverSocket);
        exit(5);
    }
    int listenResult = listen(serverSocket, 1);
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
        int clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddress, &clientAddressLen);
        if (clientSocket == -1) {
            perror("listen failed with error code");
            close(serverSocket);
            exit(6);
        }
        if (!quiteMode)printf("----New client connected----\n");
        fflush(stdin);
        startChat(clientSocket, port, true, testMode, NULL, NULL, quiteMode, ip, partA);
        close(clientSocket);
    }
    close(serverSocket);
}

/**
 * start up a tcp client and launch the chat
 * @param ip the tcp server ip
 * @param port number
 * @param testMode if -p flag is active or not
 * @param testType ipv4/ipv6/uds/mmap/pipe
 * @param testParam tcp/udp/dgram/stream/filename
 */
void clientHandler(char *ip, long port, bool testMode, char *testType, char *testParam, bool partA) {
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
    startChat(clientSocket, port, false, testMode, testType, testParam, false, ip, partA);
}

/**
 * This is the chat logic
 * using select() enable communicating between a client and a server
 * @param socket the client/server socket
 * @param port number
 * @param clientOrServer true server for false for client
 * @param testMode if -p flag is active or not
 * @param testType ipv4/ipv6/uds/mmap/pipe
 * @param testParam tcp/udp/dgram/stream/filename
 * @param quiteMode if -q flag is active or not
 */
void
startChat(int socket, long port, bool clientOrServer, bool testMode, char *testType, char *testParam, bool quiteMode,
          char *ip, bool partA) {
    bool firstMessage = true;
    fd_set readFDs, writeFDs;
    char sendBuffer[1024] = {0};
    char recvBuffer[1024] = {0};

    // Set the sockfd to non-blocking testMode
    int flags = fcntl(socket, F_GETFL, 0);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);

    if (clientOrServer == false && testMode == true && !partA) {
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
    //sever sending quiteMode
    if (clientOrServer && testMode && quiteMode && !partA) {
        snprintf(sendBuffer, sizeof(sendBuffer), "true");
        ssize_t bytesSent = send(socket, sendBuffer, strlen(sendBuffer), 0);
        if (bytesSent == -1) {
            perror("send");
            exit(1);
        } else {
            memset(sendBuffer, 0, sizeof(sendBuffer));
        }
    }
    int count = 0;
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
            count++;
            ssize_t bytesRead = recv(socket, recvBuffer, sizeof(recvBuffer), 0);
            if (bytesRead == -1) {
                perror("recv");
                exit(1);
            } else if (bytesRead == 0) {
                if (!quiteMode)printf("Connection closed by remote host\n");
                return;
//                exit(0);
            } else if (bytesRead > 0) {
                recvBuffer[bytesRead] = '\0';
                if (strcmp(recvBuffer, "true") == 0) {
                    quiteMode = true;
                }
                if (!quiteMode || strchr(recvBuffer, ',')) {
                    printf("%s\n", recvBuffer);
                }
                // if the client got a ready message from the client it can
                // launch its own thread that handle the communication in -p mode
                if (!clientOrServer && testMode && firstMessage && !partA) {
                    if (strcmp(recvBuffer, "~~Ready~~!") == 0) {
                        pthread_t thread;
                        pThreadData data = malloc(sizeof(ThreadData));
                        memset(recvBuffer, 0, sizeof(recvBuffer));
                        data->socket = socket;
                        data->port = port + 1;
                        data->testParam = testParam;
                        data->testType = testType;
                        data->quiteMode = quiteMode;
                        data->ip = ip;
                        data->recvBuff = recvBuffer;
                        int rc = pthread_create(&thread, NULL, clientTransfer, data);
                        if (rc) {
                            printf("ERROR; return code from pthread_create() is %d\n", rc);
                            exit(-1);
                        }
                        firstMessage = false;
                    }
                }
                //launch a new thread that handles the server communication type
                if (clientOrServer && testMode && firstMessage && !partA) {
                    // testType and testParam are sent with " " (space) between them
                    char *token = strtok(recvBuffer, " ");
                    if (token != NULL) {
                        free(testType);
                        testType = strdup(token);
                    }
                    token = strtok(NULL, " ");
                    if (token != NULL) {
                        free(testParam);
                        testParam = strdup(token);
                        size_t len = strlen(testType);
                        if (len > 0 && !islower(testParam[len - 1]) && isascii(testParam[len - 1])) {
                            testParam[len - 1] = '\0';
                        }
                    }
                    pthread_t thread;
                    pThreadData data = malloc(sizeof(ThreadData));
                    memset(recvBuffer, 0, sizeof(recvBuffer));
                    data->socket = socket;
                    data->port = port + 1;
                    data->testParam = testParam;
                    data->testType = testType;
                    data->quiteMode = quiteMode;
                    data->recvBuff = recvBuffer;
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
                memset(sendBuffer, 0, sizeof(sendBuffer));
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readFDs)) {
            fgets(sendBuffer, sizeof(sendBuffer), stdin);
            sendBuffer[strcspn(sendBuffer, "\n")] = '\0';
        }
    }
}

/**
 * check what combination of testType and testParam the program got
 * @param testType ipv4/ipv6/uds/mmap/pipe
 * @param testParam tcp/udp/dgram/stream/filename
 * @return ipv4,tcp=1 ipv4,udp=2 ipv6,tcp=3,ipv6,udp=4
 * uds,dgram=5 uds,stream=6 mmap,filename=7 pipe,filename=8
 */
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

/**
 * this is a method that is used by another thread
 * launch the right method depending on the checkConnection return value for a client
 * @param args pThreadData
 * @return nothing
 */
void *clientTransfer(void *args) {
    pThreadData data = (pThreadData) args;
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

/**
 * this is a method that is used by another thread
 * launch the right method depending on the checkConnection return value for a server
 * @param args pThreadData
 * @return nothing
 */
void *serverTransfer(void *args) {
    pThreadData data = (pThreadData) args;
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




