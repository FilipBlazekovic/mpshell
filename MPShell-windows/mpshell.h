
#define PAYLOAD_HEADER_SIZE         10
#define MAX_PACKET_SIZE             1500
#define MAX_COMMAND_SIZE            1500
#define INITIAL_MALLOC_SIZE         8192

/* For standard MTU of 1500 bytes, the maximum data size is 1462 bytes,
 * which is set as the default value. (MTU minus 20 bytes IP header,
 * 8 bytes for the UDP/ICMP header, and 10 bytes for the PayloadHeader).
 */

#define DEFAULT_MAX_PAYLOAD_SIZE    1462
#define DEFAULT_SLEEP               100         // 100 ms
#define DEFAULT_TIMEOUT             2           // 2 s

#define PROTOCOL_TCP                1
#define PROTOCOL_UDP                2
#define PROTOCOL_ICMP               3

/* --------------------------------------------------------------------------------------------------------------- */

struct PayloadHeader
{
    uint32_t sessionID;
    uint32_t packetID;
    uint16_t payloadSize;
} __attribute__((__packed__));


struct SessionBuffer
{
    uint32_t sessionID;
    uint32_t lastPacketReceived;
};

struct CommandBuffer
{
    unsigned char *data;
    uint32_t currentReadPosition;
    uint32_t numBytesOccupied;
    uint32_t numBytesAllocated;
};

struct ResultBuffer
{
    unsigned char *data;
    uint32_t currentReadPosition;
    uint32_t numBytesOccupied;
    uint32_t numBytesAllocated;
};

/* --------------------------------------------------------------------------------------------------------------- */

void openTCPChannel(const char *host, const char *port);
void openUDPChannel(const char *host, int port);
void openICMPChannel(const char *host);
void executeCommand(const char *command, struct ResultBuffer *resultBuffer);
