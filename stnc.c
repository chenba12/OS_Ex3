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
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>

// function signatures
void errorMessage();

typedef struct {
    char *testParam;
    char *testType;
    int socket;
    long port;
} ThreadData, *pThreadData;

void clientHandler(char *ip, long port, bool testMode, char *testType, char *testParam);

void serverHandler(long port, bool testMode, bool quiteMode);

long getPort(long port, char **ptr, const char *host);

void
startChat(int socket, long port, bool clientOrServer, bool testMode, char *testType, char *testParam, bool quiteMode);


void ipv4TcpServer(pThreadData data);

void ipv4UdpServer(pThreadData data);

void ipv6TcpServer(pThreadData data);

void ipv6UdpServer(pThreadData data);

void udsDgramServer(pThreadData data);

void udsStreamServer(pThreadData data);

void mmapFileServer(pThreadData data);

void pipeFileServer(pThreadData data);

void ipv4TcpClient(pThreadData data);

void ipv4UdpClient(pThreadData data);

void ipv6TcpClient(pThreadData data);

void pipeFileClient(pThreadData data);

void mmapFileClient(pThreadData data);

void udsStreamClient(pThreadData data);

void udsDgramClient(pThreadData data);

void ipv6UdpClient(pThreadData data);

void sendFile(int fd);

void receiveFile(int clientFd);


long getCurrentTime();

void getFileAndSendTime(pThreadData data, int clientfd);

int main(int argc, char *argv[]) {
    bool serverOrClient = false;
    long port = 0;
    char *ptr;
    char *host;
    char *ip;
    bool testMode = false;
    // ipv4,ipv6,mmap,pipe,uds are in argv[5]
    char *testType = NULL;
    // udp/tcp or dgram/stream are in argv[6]
    char *testParam = NULL;
    bool quiteMode = false;

    if (argc >= 3 && argc <= 7) {
        int result = strcmp(argv[1], "-c");
        if (result == 0) {
            serverOrClient = false;
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
                } else {
                    errorMessage();
                }
            }
            clientHandler(ip, port, testMode, testType, testParam);
        } else {
            result = strcmp(argv[1], "-s");
            if (result == 0) {
                serverOrClient = true;
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
                serverHandler(port, testMode, quiteMode);
            } else {
                errorMessage();
            }
        }
    } else {
        errorMessage();
    }
    return 0;
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
        } else if (strcmp(testParam, "stream") == 0) {
            result = 6;
        }
    } else if (strcmp(testType, "mmap") == 0) {
        result = 7;
    } else if (strcmp(testType, "pipe") == 0) {
        result = 8;
    }
    return result;
}

long getCurrentTime() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

long getPort(long port, char **ptr, const char *host) {
    port = strtol(host, ptr, 10);
    if (port == 0) {
        perror("strtol failed");
        exit(2);
    }
    return port;
}

void serverHandler(long port, bool testMode, bool quiteMode) {
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
        startChat(clientSocket, port, true, testMode, NULL, NULL, quiteMode);
        close(clientSocket);
    }
    close(serverSocket);
}

void *clientTransfer(void *args) {
    pThreadData data = (ThreadData *) args;
    if (data->testType == NULL) {
        printf("sad\n");
    }
    int connectionType = checkConnection(data->testType, data->testParam);
    switch (connectionType) {
        case 1:
            ipv4TcpClient(data);
            break;
        case 2:
            ipv4UdpClient(data);
            break;
        case 3:
            ipv6TcpClient(data);
            break;
        case 4:
            ipv6UdpClient(data);
            break;
        case 5:
            udsDgramClient(data);
            break;
        case 6:
            udsStreamClient(data);
            break;
        case 7:
            mmapFileClient(data);
            break;
        case 8:
            pipeFileClient(data);
            break;
        default:
            printf("Invalid connection type\n");
            exit(1);
    }
    pthread_exit(NULL);
    return NULL;
}

void ipv6UdpClient(pThreadData data) {

}

void udsDgramClient(pThreadData data) {

}

void udsStreamClient(pThreadData data) {

}

void mmapFileClient(pThreadData data) {

}

void pipeFileClient(pThreadData data) {

}

void ipv6TcpClient(pThreadData data) {
    struct sockaddr_in6 serv_addr;

    // Create a socket for the client
    int client_socket = socket(AF_INET6, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("socket");
        exit(1);
    }

    // Fill in the server's address and port number
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin6_family = AF_INET6;
    serv_addr.sin6_port = htons(data->port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET6, "::1", &serv_addr.sin6_addr) <= 0) {
        perror("inet_pton");
        exit(1);
    }

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        exit(1);
    }

    sendFile(client_socket);

    // Close the client socket
    free(data);
    close(client_socket);
}

void ipv4UdpClient(pThreadData data) {

}

void ipv4TcpClient(pThreadData data) {
    struct sockaddr_in serv_addr;

    // Create a socket for the client
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("socket");
        exit(1);
    }

    // Fill in the server's address and port number
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(data->port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(1);
    }

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        exit(1);
    }
    sendFile(client_socket);

    // Close the client socket
    free(data);
    close(client_socket);
}

void sendFile(int fd) {// Send the file
    FILE *fp = fopen("file", "rb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    char buffer[1024] = {0};
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send(fd, buffer, bytes_read, 0) == -1) {
            perror("send");
            exit(1);
        }
    }
    fclose(fp);
}

void *serverTransfer(void *args) {
    pThreadData data = (ThreadData *) args;


    int connectionType = checkConnection(data->testType, data->testParam);
    //TODO remove
    printf("%s\n", data->testType);
    printf("%s\n", data->testParam);
    switch (connectionType) {
        case 1:
            ipv4TcpServer(data);
            break;
        case 2:
            ipv4UdpServer(data);
            break;
        case 3:
            ipv6TcpServer(data);
            break;
        case 4:
            ipv6UdpServer(data);
            break;
        case 5:
            udsDgramServer(data);
            break;
        case 6:
            udsStreamServer(data);
            break;
        case 7:
            mmapFileServer(data);
            break;
        case 8:
            pipeFileServer(data);
            break;
        default:
            printf("Invalid connection type\n");
            exit(1);
    }
    exit(1);
}

void pipeFileServer(pThreadData data) {

}

void mmapFileServer(pThreadData data) {

}

void udsStreamServer(pThreadData data) {

}

void udsDgramServer(pThreadData data) {

}

void ipv6UdpServer(pThreadData data) {

}

void ipv6TcpServer(pThreadData data) {
    // Open a new TCP socket
    int ipv6tcpSocket = socket(AF_INET6, SOCK_STREAM, 0);
    if (ipv6tcpSocket == -1) {
        perror("socket");
        exit(1);
    }

    // Set the socket options
    int optval = 1;
    if (setsockopt(ipv6tcpSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    // Bind the socket to the specified address and port
    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_loopback; // Use loopback address
    addr.sin6_port = htons(data->port); // Add 1 to the port number
    if (bind(ipv6tcpSocket, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(1);
    }

    // Listen for incoming connections
    if (listen(ipv6tcpSocket, 5) == -1) {
        perror("listen");
        exit(1);
    }

    // Modify the string
    char readyStr[200];
    snprintf(readyStr, sizeof(readyStr), "~~Ready~~!");

    // Send the string over the socket
    send(data->socket, readyStr, strlen(readyStr), 0);

    // Accept incoming connections and handle them in a loop
    while (1) {
        struct sockaddr_in6 client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int clientfd = accept(ipv6tcpSocket, (struct sockaddr *) &client_addr, &addrlen);
        if (clientfd == -1) {
            perror("accept");
            continue;
        }
        // Receive the file
        // Example usage to measure elapsed time
        getFileAndSendTime(data, clientfd);
        close(clientfd);
        break;
    }

    // Close the socket
    close(ipv6tcpSocket);
}

void ipv4UdpServer(pThreadData data) {

}

void ipv4TcpServer(pThreadData data) {// Open a new TCP socket
    int ipv4tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (ipv4tcpSocket == -1) {
        perror("socket");
        exit(1);
    }

    // Set the socket options
    int optval = 1;
    if (setsockopt(ipv4tcpSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    // Bind the socket to the specified address and port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // Use loopback address
    addr.sin_port = htons(data->port); // Add 1 to the port number
    if (bind(ipv4tcpSocket, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(1);
    }

    // Listen for incoming connections
    if (listen(ipv4tcpSocket, 5) == -1) {
        perror("listen");
        exit(1);
    }

    // Modify the string
    char readyStr[200];
    snprintf(readyStr, sizeof(readyStr), "~~Ready~~!");

    // Send the string over the socket
    send(data->socket, readyStr, strlen(readyStr), 0);

    // Accept incoming connections and handle them in a loop
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int clientfd = accept(ipv4tcpSocket, (struct sockaddr *) &client_addr, &addrlen);
        if (clientfd == -1) {
            perror("accept");
            continue;
        }
        // Receive the file
        // Example usage to measure elapsed time
        getFileAndSendTime(data, clientfd);
        close(clientfd);
        break;
    }
    // Close the socket
    close(ipv4tcpSocket);
}

void getFileAndSendTime(pThreadData data, int clientfd) {
    long startTime = getCurrentTime();
    receiveFile(clientfd);
    long endTime = getCurrentTime();
    long elapsedTime = endTime - startTime;
    char elapsedStr[200];
    snprintf(elapsedStr, sizeof(elapsedStr), "%s_%s,%ld\n", data->testType, data->testParam, elapsedTime);
    printf("%s", elapsedStr);
    send(data->socket, elapsedStr, strlen(elapsedStr), 0);
}

void receiveFile(int clientFd) {
    FILE *fp = fopen("received_file", "wb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    char buffer[1024] = {0};
    size_t bytes_read;
    while ((bytes_read = recv(clientFd, buffer, sizeof(buffer), 0)) > 0) {
        if (fwrite(buffer, 1, bytes_read, fp) != bytes_read) {
            perror("fwrite");
            exit(1);
        }
    }
    if (bytes_read == -1) {
        perror("recv");
        exit(1);
    }
    fclose(fp);
}

void
startChat(int socket, long port, bool clientOrServer, bool testMode, char *testType, char *testParam, bool quiteMode) {
    bool firstMessage = true;
    fd_set readfds, writefds;
    char sendbuf[1024] = {0};
    char recvbuf[1024] = {0};

    // Set the sockfd to non-blocking testMode
    int flags = fcntl(socket, F_GETFL, 0);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);

    if (clientOrServer == false && testMode == true) {
        if (testType != NULL && testParam != NULL) {
            snprintf(sendbuf, sizeof(sendbuf), "%s %s\n", testType, testParam);
        }
        int numbytes = send(socket, sendbuf, strlen(sendbuf), 0);
        if (numbytes == -1) {
            perror("send");
            exit(1);
        } else {
            memset(sendbuf, 0, sizeof(sendbuf));
        }
    }

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
                if (!quiteMode) {
                    printf("Received message: %s\n", recvbuf);
                }
                if (!clientOrServer && testMode && firstMessage) {
                    if (strcmp(recvbuf, "~~Ready~~!") == 0) {
                        pthread_t thread;
                        pThreadData data = malloc(sizeof(ThreadData));
                        memset(recvbuf, 0, sizeof(recvbuf));
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
                    char *token = strtok(recvbuf, " ");
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
                    memset(recvbuf, 0, sizeof(recvbuf));
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
