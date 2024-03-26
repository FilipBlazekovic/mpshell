#include "mplistener.h"

extern unsigned char *packet_in;
extern unsigned char *packet_out;

extern atomic_bool RUN;

static unsigned short calculateChecksum(unsigned short *start_address, int length)
{
    int sum = 0;
    int bytes_remaining = length;
    unsigned short *current_value = start_address;
    unsigned short oddbyte = 0;
    unsigned short answer;

    while (bytes_remaining > 1)
    {
        sum += *current_value++;
        bytes_remaining -= 2;
    }
    if (bytes_remaining == 1)
    {
        *(unsigned char *) (&oddbyte) = *(unsigned char *) current_value;
        sum += oddbyte;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;
    return answer;
}

int start_icmp_listener(void)
{
    int sockfd;
    struct sockaddr_in client_address;
    memset(&client_address, 0, sizeof(client_address));
    socklen_t client_address_size = sizeof(client_address);

    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
    {
        perror("[-] Socket creation failed => ");
        return EXIT_FAILURE;
    }

    // Setting a filter to receive only ICMP request packets
    struct icmp_filter filter;
    filter.data = ~(1<<ICMP_ECHO);
    if (setsockopt(sockfd, SOL_RAW, ICMP_FILTER, &filter, sizeof(filter)) < 0)
    {
        perror("[-] ICMP filter setting failed => ");
        return EXIT_FAILURE;
    }

    struct icmphdr *icmp_header_in  = (struct icmphdr *) (packet_in + sizeof(struct iphdr));
    mp_payload_header *header_in    = (mp_payload_header *) (packet_in + sizeof(struct iphdr) + sizeof(struct icmphdr));
    unsigned char *data_in          = (packet_in + sizeof(struct iphdr) + sizeof(struct icmphdr) + sizeof(mp_payload_header));

    // The response doesn't need the IP header at the
    // beginning of the packet, kernel adds the IP header
    struct icmphdr *icmp_header_out = (struct icmphdr *) packet_out;
    mp_payload_header *header_out   = (mp_payload_header *) (packet_out + sizeof(struct icmphdr));
    unsigned char *data_out         = (packet_out + sizeof(struct icmphdr) + sizeof(mp_payload_header));

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

        // Set ICMP Header values, IP header is populated by the kernel
        size_t response_size              = sizeof(struct icmphdr) + PAYLOAD_HEADER_SIZE + ntohs(header_out->payload_size);

        icmp_header_out->type             = ICMP_ECHOREPLY;
        icmp_header_out->code             = 0;
        icmp_header_out->un.echo.id       = icmp_header_in->un.echo.id;
        icmp_header_out->un.echo.sequence = icmp_header_in->un.echo.sequence;
        icmp_header_out->checksum         = 0;
        icmp_header_out->checksum         = calculateChecksum((unsigned short *) icmp_header_out, response_size);

        sendto(sockfd, packet_out, response_size, 0, (struct sockaddr *)&client_address, client_address_size);
    }

    close(sockfd);
    return EXIT_SUCCESS;
}
