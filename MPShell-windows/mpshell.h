#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <time.h>

#include "../MPCommon/mpshell-common.h"

void execute_command(const char *command);

void process_packet(
    ssize_t num_bytes_received,
    mp_payload_header *header_in,
    mp_payload_header *header_out,
    unsigned char *data_in,
    unsigned char *data_out
);

int open_tcp_channel(const char *host, const char *port);
int open_udp_channel(const char *host, int port);
int open_icmp_channel(const char *host);
