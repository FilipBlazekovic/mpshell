#include "mpshell.h"

extern unsigned char *packet_in;
extern unsigned char *packet_out;

extern bool RUN;
extern uint16_t max_payload_size;
extern time_t timeout;

int open_udp_channel(const char *host, int port)
{
    int sockfd;
    ssize_t num_bytes_received;
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(host);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("[ERROR] Socket creation failed => ");
        return EXIT_FAILURE;
    }

    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        perror("[ERROR] Could not set read timeout => ");
        return EXIT_FAILURE;
    }

    mp_payload_header *header_in    = (mp_payload_header *) packet_in;
    mp_payload_header *header_out   = (mp_payload_header *) packet_out;

    unsigned char *data_in          = (packet_in + sizeof(mp_payload_header));
    unsigned char *data_out         = (packet_out + sizeof(mp_payload_header));

    srand(time(NULL));
    uint16_t random_session_id      = (uint16_t) (1 + rand()%65535);

    header_out->session_id          = htonl(random_session_id);
    header_out->packet_id           = htonl(1);
    header_out->payload_size        = htons(4);

    // First packet contains a payload of 4
    // bytes with the desired max_payload_size
    uint32_t temp = htonl(max_payload_size);
    memcpy(data_out, &temp, 4);

    while (RUN)
    {
        sendto(
            sockfd,
            packet_out,
            (PAYLOAD_HEADER_SIZE + ntohs(header_out->payload_size)),
            0,
            (const struct sockaddr *) &server_address,
            sizeof(server_address)
        );

        num_bytes_received = recvfrom(sockfd, (unsigned char *) packet_in, MAX_PACKET_SIZE, MSG_WAITALL, NULL, NULL);
        process_packet(num_bytes_received, NULL, header_in, header_out, data_in, data_out);
    }

    close(sockfd);
    return EXIT_SUCCESS;
}
