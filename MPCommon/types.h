#include <stdint.h>

#define PAYLOAD_HEADER_SIZE         10
#define MAX_PACKET_SIZE             1500
#define MAX_COMMAND_SIZE            1500
#define INITIAL_MALLOC_SIZE         8192

// For standard MTU of 1500 bytes, the maximum data size is 1462 bytes,
// which is set as the default value. (MTU minus 20 bytes IP header,
// 8 bytes for the UDP/ICMP header, and 10 bytes for the payload_header).
#define DEFAULT_MAX_PAYLOAD_SIZE    1462

enum mp_protocol {
    PROTOCOL_TCP,
    PROTOCOL_UDP,
    PROTOCOL_ICMP
};

struct mp_payload_header {
    uint32_t session_id;
    uint32_t packet_id;
    uint16_t payload_size;
} __attribute__((__packed__));

struct mp_session_buffer {
    uint32_t session_id;
    uint32_t last_packet_received;
};

struct mp_command_buffer {
    unsigned char *data;
    uint32_t current_read_position;
    uint32_t num_bytes_occupied;
    uint32_t num_bytes_allocated;
};

struct mp_result_buffer {
    unsigned char *data;
    uint32_t current_read_position;
    uint32_t num_bytes_occupied;
    uint32_t num_bytes_allocated;
};

typedef struct mp_payload_header mp_payload_header;
typedef struct mp_session_buffer mp_session_buffer;
typedef struct mp_command_buffer mp_command_buffer;
typedef struct mp_result_buffer mp_result_buffer;
