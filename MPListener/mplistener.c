#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <errno.h>

#include "mplistener.h"

/* --------------------------------------------------------------------------------------------------------------- */

FILE *commandInputFile = NULL;
FILE *resultOutputFile = NULL;

char *commandFilePath  = NULL;
char *resultFilePath   = NULL;

struct SessionBuffer *sessionBuffer;
struct CommandBuffer *commandBuffer;
struct ResultBuffer *resultBuffer;

pthread_t inputThreadID;
pthread_mutex_t lock;
pthread_cond_t cv;

atomic_bool COMMAND_PAYLOAD_READY  = false;
atomic_bool RUN                    = true;

int port                           = -1;
int protocol                       = PROTOCOL_TCP;
uint32_t maxPayloadSize            = DEFAULT_MAX_PAYLOAD_SIZE;
char clientIP[32]                  = "UNKNOWN";

/* --------------------------------------------------------------------------------------------------------------- */

void signalHandler(int signum) { RUN = false; }
void showUsage()
{
    const char *usageString =
    "\nUSAGE: ./MPListener --protocol=tcp|udp|icmp [OPTIONS]\n\n" \
    "Options:\n" \
    "-------\n" \
    "--port=8080\n" \
    "--command-file=/path/to/file\n" \
    "--result-file=/path/to/file\n\n";

    printf(usageString);
    exit(1);
}

/* --------------------------------------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

int main(int argc, const char *argv[])
{

    // Parsing command-line arguments
    // ------------------------------
    char delimiter[] = "=";
    for (int i = 1; i < argc; i++)
    {
        char *paramName  = NULL;
        char *paramValue = NULL;

        char *currentArgument = argv[i];
        paramName = strtok(currentArgument, delimiter);

        if (paramName != NULL)
            paramValue = strtok(NULL, delimiter);

        if (paramName != NULL && paramValue != NULL)
        {
            if (strcmp(paramName, "--port") == 0)               { port = atoi(paramValue); }
            else if (strcmp(paramName, "--protocol") == 0)
            {
                if (strcmp(paramValue, "tcp") == 0)             { protocol = PROTOCOL_TCP; }
                else if (strcmp(paramValue, "udp") == 0)        { protocol = PROTOCOL_UDP; }
                else if (strcmp(paramValue, "icmp") == 0)       { protocol = PROTOCOL_ICMP; }
                else { showUsage(); }
            }
            else if (strcmp(paramName, "--command-file") == 0)  { commandFilePath = paramValue; }
            else if (strcmp(paramName, "--result-file") == 0)   { resultFilePath  = paramValue; }
            else { showUsage(); }
        }
        else { showUsage(); }
    }


    // Validating arguments
    // --------------------
    if ((protocol != PROTOCOL_ICMP) && (port <= 0 || port > 65535))
    {
        showUsage();
    }


    // Starting reverse shell listener
    // -------------------------------
    printf("************************************************************\n");
    printf("*%-58s*\n", "");
    printf("*%-14s%s%-14s*\n","", "REVERSE SHELL LISTENER STARTED", "");
    printf("*%-58s*\n", "");
    printf("************************************************************\n");
    printf("\n");

    if (protocol != PROTOCOL_ICMP)
        printf("[+] LISTENING ON PORT: %d\n", port);

    if (protocol == PROTOCOL_TCP)
        printf("[+] TRANSFER PROTOCOL: TCP\n");
    else if (protocol == PROTOCOL_UDP)
        printf("[+] TRANSFER PROTOCOL: UDP\n");
    else
    {
        printf("[+] TRANSFER PROTOCOL: ICMP\n");
        printf("[+] Disabling automatic kernel ping replies ...\n");
        int status = system("echo '1' > /proc/sys/net/ipv4/icmp_echo_ignore_all");
        if (status != 0)
        {
            printf("[-] Could not disable automatic kernel ping replies. Exiting...\n");
            exit(1);
        }
    }
    if (commandFilePath != NULL)
    {
        printf("[+] Opening command input file ...\n");
        commandInputFile = fopen(commandFilePath, "r");
        if (commandInputFile == NULL)
            printf("[-] Could not open file! Fallback to stdin ...\n");
    }
    if (resultFilePath != NULL)
    {
        printf("[+] Opening result output file ...\n");
        resultOutputFile = fopen(resultFilePath, "w");
        if (resultOutputFile == NULL)
            printf("[-] Could not open file! Fallback to stdout ...\n");
    }


    // Installing a SIGINT signal handler
    // ----------------------------------
    printf("[+] Installing SIGINT handler ...\n");
    signal(SIGINT, signalHandler);


    // Starting the listener
    // ---------------------
    switch (protocol)
    {
        case PROTOCOL_TCP:  startTCPListener(port); break;
        case PROTOCOL_UDP:  startUDPListener(port); break;
        case PROTOCOL_ICMP: startICMPListener();    break;
    }

    // Cleanup
    // -------
    if (commandInputFile != NULL)
        fclose(commandInputFile);
    if (resultOutputFile != NULL)
        fclose(resultOutputFile);
}

/* --------------------------------------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

void readCommand()
{
    COMMAND_PAYLOAD_READY = false;
    while (RUN)
    {
        printf("[%s] >> ", clientIP);

        char command[MAX_COMMAND_SIZE] = {0};
        if (commandInputFile != NULL)
        {
            /* On EOF/Error redirect input to stdin */
            if ((fgets(command, sizeof(command), commandInputFile)) == NULL)
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

        memcpy(commandBuffer->data, &command[0], strlen(command)+1);
        commandBuffer->numBytesOccupied = strlen(command)+1;
        printf("Waiting for response...\n");

        break;
    }
    COMMAND_PAYLOAD_READY = true;
}

/* --------------------------------------------------------------------------------------------------------------- */

void *inputHandler(void *data)
{
    readCommand();
    while (RUN)
    {
        pthread_mutex_lock(&lock);
        pthread_cond_wait(&cv, &lock);
        readCommand();
        pthread_mutex_unlock(&lock);

        if ((strcmp(commandBuffer->data, "quit") == 0) || (strcmp(commandBuffer->data, "exit") == 0))
            break;
    }
}

/* --------------------------------------------------------------------------------------------------------------- */
