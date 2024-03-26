#include <arpa/inet.h>
#include <errno.h>
#include <linux/icmp.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../MPCommon/types.h"

int start_tcp_listener(int port);
int start_udp_listener(int port);
int start_icmp_listener(void);

bool process_packet(
    struct sockaddr_in client_address,
    ssize_t num_bytes_received,
    mp_payload_header *header_in,
    mp_payload_header *header_out,
    unsigned char *data_in,
    unsigned char *data_out
);
