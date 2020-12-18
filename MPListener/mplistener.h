
#define PAYLOAD_HEADER_SIZE         10
#define MAX_PACKET_SIZE             1500
#define MAX_COMMAND_SIZE            1500
#define INITIAL_MALLOC_SIZE         8192
#define DEFAULT_MAX_PAYLOAD_SIZE    1462

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

void startTCPListener(int port);
void startUDPListener(int port);
void startICMPListener();

void *inputHandler(void *data);
unsigned short calculateChecksum(unsigned short *startAddress, int length);
