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

extern atomic_bool RUN;
extern char clientIP[32];

extern FILE *commandInputFile;
extern FILE *resultOutputFile;

/* --------------------------------------------------------------------------------------------------------------- */

void startTCPListener(int port)
{

    int sockfd;
    int clientSocket;
    struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;

    memset(&serverAddress, 0, sizeof(serverAddress));
    memset(&clientAddress, 0, sizeof(clientAddress));

    serverAddress.sin_family      = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port        = htons(port);
    socklen_t clientAddressSize   = sizeof(clientAddress);

    ssize_t totalBytesSent;
    ssize_t numBytesSent;
    ssize_t totalBytesRead;
    ssize_t numBytesRead;


    // Creating a server socket
    // ------------------------
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("[-] Socket creation failed => ");
        exit(EXIT_FAILURE);
    }


    // Binding a server socket
    // -----------------------
    if (bind(sockfd, (const struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("[-] Bind failed => ");
        close(sockfd);
        exit(EXIT_FAILURE);
    }


    // Listening on a socket
    // ---------------------
    if (listen(sockfd, 3) < 0)
    {
        perror("[-] Listen failed => ");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("[+] Waiting for connection ...\n");


    // Accepting a connection
    // ----------------------
    clientSocket = accept(sockfd, (struct sockaddr *)&clientAddress, &clientAddressSize);
    if (clientSocket < 0)
    {
        perror("[-] Accept failed => ");
        close(sockfd);
        exit(EXIT_FAILURE);
    }


    // Extracting remote IP address
    // ----------------------------
    struct sockaddr_in *address = (struct sockaddr_in *)&clientAddress;
    char *ip = inet_ntoa(address->sin_addr);
    if (ip != NULL)
        memcpy(clientIP, ip, strlen(ip));
    printf("[+] Received connection from %s\n\n", clientIP);



    // Main loop
    // ---------
    while (RUN)
    {

        /* ------------------------------------------------------------------------------------- */

        // Input command
        // -------------
        printf("[%s] >> ", clientIP);

        char command[MAX_COMMAND_SIZE] = {0};
        if (commandInputFile != NULL)
        {
            /* On EOF/Error redirect input to stdin */
            char *data = fgets(command, sizeof(command), commandInputFile);
            if (data == NULL)
            {
                fclose(commandInputFile);
                commandInputFile = NULL;
                printf("\n");
                continue;
            }
        }
        else { fgets(command, sizeof(command), stdin); }

        /* only newline is present */
        if (strlen(command) <= 1)
        {
            printf("\n");
            continue;
        }

        /* replace newline with null character */
        command[strlen(command)-1] = '\0';

        /* if command is $close-output close output
         * file which redirects output to stdout
         */
        if (strcmp(command, "$close-output") == 0)
        {
            fclose(resultOutputFile);
            resultOutputFile = NULL;
            continue;
        }

        /* If command input from file is specified echo the command on the command line */
        if (commandInputFile != NULL) { puts(command); }

        /* If result output to file is specified echo the command back to file */
        if (resultOutputFile != NULL) { fprintf(resultOutputFile, "COMMAND => %s\n", command); }

        /* ------------------------------------------------------------------------------------- */

        // Send the command
        // ----------------
        totalBytesSent            = 0;
        numBytesSent              = 0;
        uint32_t commandLength    = strlen(command)+1;
        uint32_t netCommandLength = htonl(commandLength); 

        numBytesSent = send(clientSocket, &netCommandLength, 4, 0);
        if (numBytesSent < 4)
        {
            perror("[-] ERROR: ");
            break;
        }
        while (totalBytesSent < commandLength)
        {
            numBytesSent = send(clientSocket, &command[totalBytesSent], (commandLength-totalBytesSent), 0);
            if (numBytesSent > 0)
                totalBytesSent += numBytesSent;
        }

        if ((strcmp(command, "quit") == 0) || (strcmp(command, "exit") == 0))
            break;

        /* ------------------------------------------------------------------------------------- */

        // Read response
        // -------------
        printf("Waiting for response...\n");

        totalBytesRead = 0;
        numBytesRead   = 0;
        uint32_t temp  = 0;

        numBytesRead = recv(clientSocket, &temp, 4, 0); 
        if (numBytesRead < 4)
        {
            perror("[-] ERROR: ");
            break;
        }

        uint32_t responseLength = ntohl(temp);
        char response[responseLength];
        while (totalBytesRead < responseLength)
        {
            numBytesRead = recv(clientSocket, &response[totalBytesRead], (responseLength-totalBytesRead), 0);
            if (numBytesRead > 0)
                totalBytesRead += numBytesRead;
        }

        if (resultOutputFile != NULL)
            fprintf(resultOutputFile, "%s\n", response);
        else
            printf("%s\n", response);
    }

    close(clientSocket);
    close(sockfd);
}

/* --------------------------------------------------------------------------------------------------------------- */
