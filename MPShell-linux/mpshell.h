#include <arpa/inet.h>
#include <linux/icmp.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include "../MPCommon/mpshell-common.h"

void execute_command(const char *command);

void process_packet(
    ssize_t num_bytes_received,
    struct icmphdr *icmp_header_out,
    mp_payload_header *header_in,
    mp_payload_header *header_out,
    unsigned char *data_in,
    unsigned char *data_out
);

int open_tcp_channel(const char *host, int port);
int open_udp_channel(const char *host, int port);
int open_icmp_channel(const char *host);
