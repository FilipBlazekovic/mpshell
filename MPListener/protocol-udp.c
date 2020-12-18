#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <errno.h>

#include "mplistener.h"

extern struct SessionBuffer *sessionBuffer;
extern struct CommandBuffer *commandBuffer;
extern struct ResultBuffer *resultBuffer;

extern char clientIP[32];
extern uint32_t maxPayloadSize;

extern pthread_t inputThreadID;
extern pthread_cond_t cv;

extern atomic_bool COMMAND_PAYLOAD_READY;
extern atomic_bool RUN;

extern FILE *resultOutputFile;

/* --------------------------------------------------------------------------------------------------------------- */

void startUDPListener(int port)
{

    int sockfd;
    ssize_t numBytesReceived;
    struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;

    memset(&serverAddress, 0, sizeof(serverAddress));
    memset(&clientAddress, 0, sizeof(clientAddress));

    serverAddress.sin_family      = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port        = htons(port);
    socklen_t clientAddressSize   = sizeof(clientAddress);


    // Creating a server socket
    // ------------------------
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("[-] Socket creation failed => ");
        exit(EXIT_FAILURE);
    }


    // Binding a server socket
    // -----------------------
    if (bind(sockfd, (const struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("[-] Bind failed => ");
        exit(EXIT_FAILURE);
    }


    // Initiating buffers
    // ------------------
    sessionBuffer = (struct SessionBuffer *) malloc(sizeof(struct SessionBuffer));
    memset(sessionBuffer, 0, sizeof(struct SessionBuffer));

    commandBuffer = (struct CommandBuffer *) malloc(sizeof(struct CommandBuffer));
    commandBuffer->data                 = (unsigned char *) malloc(MAX_COMMAND_SIZE);
    commandBuffer->currentReadPosition  = 0;
    commandBuffer->numBytesOccupied     = 0;
    commandBuffer->numBytesAllocated    = MAX_COMMAND_SIZE;

    resultBuffer = (struct ResultBuffer *) malloc(sizeof(struct ResultBuffer));
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


    // Main loop
    // ---------
    printf("[+] Waiting for connection ...\n");
    while (RUN)
    {

        numBytesReceived = recvfrom(sockfd, (unsigned char *)packetIN, MAX_PACKET_SIZE, MSG_WAITALL, (struct sockaddr *) &clientAddress, &clientAddressSize); 
        if (numBytesReceived >= sizeof(struct PayloadHeader))
        {
            uint32_t sessionID   = ntohl(headerIN->sessionID);
            uint32_t packetID    = ntohl(headerIN->packetID);
            uint16_t payloadSize = ntohs(headerIN->payloadSize);


            // First-packet == session initialization
            // --------------------------------------
            if (packetID == 1 && sessionBuffer->sessionID == 0)
            {
                /* Initiate session buffer */
                sessionBuffer->sessionID          = sessionID;
                sessionBuffer->lastPacketReceived = packetID;

                /* Initiate output headers */
                headerOUT->sessionID   = htonl(sessionID);
                headerOUT->packetID    = htonl(packetID);
                headerOUT->payloadSize = 0;

                /* Extract the desired maxPayloadSize
                 * The first packet contains only 4 bytes
                 * that specify the desired max payload size
                 */
                if (payloadSize == 4)
                {
                    uint32_t *temp = (uint32_t*) &dataIN[0];
                    uint32_t tempValue = ntohl(*temp);
                    if (tempValue > 0 && tempValue < DEFAULT_MAX_PAYLOAD_SIZE)
                        maxPayloadSize = tempValue;
                }

                /* Extract remote IP address */
                struct sockaddr_in *address = (struct sockaddr_in *)&clientAddress;
                char *ip = inet_ntoa(address->sin_addr);
                if (ip != NULL)
                    memcpy(clientIP, ip, strlen(ip));

                printf("[+] Received connection from %s\n\n", clientIP);

                if ((pthread_create(&inputThreadID, NULL, &inputHandler, NULL)) != 0)
                {
                    printf("[-] Could not create input thread! Exiting ...");
                    RUN = false;
                }
                else { pthread_cond_signal(&cv); }
            }


            // Other packets
            // -------------
            else if (sessionBuffer->sessionID == sessionID)
            {
                if (packetID == (sessionBuffer->lastPacketReceived + 1))
                {
                    sessionBuffer->lastPacketReceived = packetID;
                    if (payloadSize > 0)
                    {
                        if (payloadSize >= (resultBuffer->numBytesAllocated - resultBuffer->numBytesOccupied))
                        {
                            size_t newAllocatedSize = (2 * resultBuffer->numBytesAllocated);
                            resultBuffer->data = (unsigned char *)realloc(resultBuffer->data, newAllocatedSize);
                            resultBuffer->numBytesAllocated = newAllocatedSize;
                        }

                        memcpy(&resultBuffer->data[resultBuffer->numBytesOccupied], dataIN, payloadSize);
                        resultBuffer->numBytesOccupied += payloadSize;
                    }
                    else
                    {
                        /* If there is anything in the result buffer
                         * on first new empty packet output it to screen,
                         * clear buffer, and wake up command input thread.
                         */
                        if (resultBuffer->numBytesOccupied > 0)
                        {
                            /* Output the result to file/stdout */
                            if (resultOutputFile != NULL)
                                fprintf(resultOutputFile, "%.*s", resultBuffer->numBytesOccupied, resultBuffer->data);
                            else 
                                printf("%.*s", resultBuffer->numBytesOccupied, resultBuffer->data);

                            /* Clear the result buffer */
                            memset(resultBuffer->data, 0, resultBuffer->numBytesOccupied);
                            resultBuffer->numBytesOccupied = 0;

                            pthread_cond_signal(&cv);
                        }
                    }

                    /* Set new payload if present or clear the old one if not.
                     * COMMAND_PAYLOAD_READY will be true after the command
                     * has been entered at the command prompt and saved into
                     * commandBuffer.
                     */
                    headerOUT->payloadSize = 0;
                    if (COMMAND_PAYLOAD_READY)
                    {
                        if (commandBuffer->numBytesOccupied > 0)
                        {
                            // if command is quit/exit exit the loop after sending the packet
                            if ((strcmp(commandBuffer->data, "quit") == 0) || (strcmp(commandBuffer->data, "exit") == 0))
                                RUN = false;

                            if (commandBuffer->numBytesOccupied > maxPayloadSize)
                            {
                                headerOUT->payloadSize = htons(maxPayloadSize);
                                memcpy(dataOUT, &commandBuffer->data[commandBuffer->currentReadPosition], maxPayloadSize);
                                commandBuffer->currentReadPosition += maxPayloadSize;
                                commandBuffer->numBytesOccupied    -= maxPayloadSize;
                            }
                            else
                            {
                                headerOUT->payloadSize = htons(commandBuffer->numBytesOccupied);
                                memcpy(dataOUT, &commandBuffer->data[commandBuffer->currentReadPosition], commandBuffer->numBytesOccupied);
                                commandBuffer->currentReadPosition = 0;
                                commandBuffer->numBytesOccupied    = 0;
                            }
                        }
                    }
                }
            }
            else { continue; }

            headerOUT->packetID = htonl(sessionBuffer->lastPacketReceived);
            sendto(sockfd, packetOUT, (PAYLOAD_HEADER_SIZE + ntohs(headerOUT->payloadSize)), 0, (struct sockaddr *)&clientAddress, clientAddressSize);
        }
    }


    free(commandBuffer->data);
    free(commandBuffer);
    free(resultBuffer->data);
    free(resultBuffer);
    free(sessionBuffer);

    close(sockfd);
}

/* --------------------------------------------------------------------------------------------------------------- */
