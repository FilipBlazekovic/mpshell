#include "mplistener.h"

extern atomic_bool RUN;
extern char client_ip[32];

extern FILE *command_input_file;
extern FILE *result_output_file;

int start_tcp_listener(int port)
{
    int sockfd;
    int client_socket;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;

    memset(&server_address, 0, sizeof(server_address));
    memset(&client_address, 0, sizeof(client_address));

    server_address.sin_family      = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port        = htons(port);
    socklen_t client_address_size  = sizeof(client_address);

    ssize_t total_bytes_sent;
    ssize_t num_bytes_sent;
    ssize_t total_bytes_read;
    ssize_t num_bytes_read;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("[-] Socket creation failed => ");
        return EXIT_FAILURE;
    }

    if (bind(sockfd, (const struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("[-] Bind failed => ");
        close(sockfd);
        return EXIT_FAILURE;
    }

    if (listen(sockfd, 3) < 0)
    {
        perror("[-] Listen failed => ");
        close(sockfd);
        return EXIT_FAILURE;
    }
    printf("[+] Waiting for connection...\n");

    client_socket = accept(sockfd, (struct sockaddr *)&client_address, &client_address_size);
    if (client_socket < 0)
    {
        perror("[-] Accept failed => ");
        close(sockfd);
        return EXIT_FAILURE;
    }

    struct sockaddr_in *address = (struct sockaddr_in *)&client_address;
    char *ip = inet_ntoa(address->sin_addr);
    if (ip != NULL)
        memcpy(client_ip, ip, strlen(ip));
    printf("[+] Received connection from %s\n\n", client_ip);

    while (RUN)
    {
        printf("[%s] >> ", client_ip);

        char command[MAX_COMMAND_SIZE] = {0};
        if (command_input_file != NULL)
        {
            // On EOF/Error redirect input to stdin
            char *data = fgets(command, sizeof(command), command_input_file);
            if (data == NULL)
            {
                fclose(command_input_file);
                command_input_file = NULL;
                printf("\n");
                continue;
            }
        }
        else { fgets(command, sizeof(command), stdin); }

        // only newline is present
        if (strlen(command) <= 1)
        {
            printf("\n");
            continue;
        }

        // replace newline with null character
        command[strlen(command)-1] = '\0';

        // if command is $close-output close output
        // file which redirects output to stdout
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

        total_bytes_sent            = 0;
        num_bytes_sent              = 0;
        uint32_t command_length     = strlen(command)+1;
        uint32_t net_command_length = htonl(command_length);

        num_bytes_sent = send(client_socket, &net_command_length, 4, 0);
        if (num_bytes_sent < 4)
        {
            perror("[-] ERROR: ");
            break;
        }
        while (total_bytes_sent < command_length)
        {
            num_bytes_sent = send(client_socket, &command[total_bytes_sent], (command_length-total_bytes_sent), 0);
            if (num_bytes_sent > 0)
                total_bytes_sent += num_bytes_sent;
        }

        if ((strcmp(command, "quit") == 0) || (strcmp(command, "exit") == 0))
            break;

        printf("Waiting for response...\n");

        total_bytes_read = 0;
        num_bytes_read   = 0;
        uint32_t temp    = 0;

        num_bytes_read = recv(client_socket, &temp, 4, 0);
        if (num_bytes_read < 4)
        {
            perror("[-] ERROR: ");
            break;
        }

        uint32_t response_length = ntohl(temp);
        char response[response_length];
        while (total_bytes_read < response_length)
        {
            num_bytes_read = recv(client_socket, &response[total_bytes_read], (response_length-total_bytes_read), 0);
            if (num_bytes_read > 0)
                total_bytes_read += num_bytes_read;
        }

        if (result_output_file != NULL)
            fprintf(result_output_file, "%s\n", response);
        else
            printf("%s\n", response);
    }

    close(client_socket);
    close(sockfd);
    return EXIT_SUCCESS;
}
