#include "mplistener.h"

extern unsigned char *packet_in;
extern unsigned char *packet_out;

extern atomic_bool RUN;

int start_udp_listener(int port)
{
    int sockfd;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;

    memset(&server_address, 0, sizeof(server_address));
    memset(&client_address, 0, sizeof(client_address));

    server_address.sin_family      = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port        = htons(port);
    socklen_t client_address_size  = sizeof(client_address);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("[-] Socket creation failed => ");
        return EXIT_FAILURE;
    }

    if (bind(sockfd, (const struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("[-] Bind failed => ");
        return EXIT_FAILURE;
    }

    mp_payload_header *header_in  = (mp_payload_header *) packet_in;
    mp_payload_header *header_out = (mp_payload_header *) packet_out;
    unsigned char *data_in        = (packet_in + sizeof(mp_payload_header));
    unsigned char *data_out       = (packet_out + sizeof(mp_payload_header));

    printf("[+] Waiting for connection...\n");
    while (RUN)
    {
        ssize_t num_bytes_received  = recvfrom(
            sockfd,
            (unsigned char *) packet_in,
            MAX_PACKET_SIZE,
            MSG_WAITALL,
            (struct sockaddr *) &client_address,
            &client_address_size
        );

        if (!process_packet(client_address, num_bytes_received, header_in, header_out, data_in, data_out))
            continue;

        sendto(
            sockfd,
            packet_out,
            (PAYLOAD_HEADER_SIZE + ntohs(header_out->payload_size)),
            0,
            (struct sockaddr *) &client_address,
            client_address_size
        );
    }

    close(sockfd);
    return EXIT_SUCCESS;
}
