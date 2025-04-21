#include <stdio.h>

#include "sender.h"
#include "utils.h"
#include "receiver.h"
#include "logger.h"

void send_sha256_acknowledgement(SOCKET sockfd, const char *server_ip,
uint16_t server_port, uint8_t status, uint32_t transmission_id) {
    struct sockaddr_in server_addr;
    uint8_t ack_packet[BUFFER_SIZE];  // Buffer to hold the acknowledgment packet
    int ack_packet_size = 0;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);  // Convert to network byte order
    server_addr.sin_addr.s_addr = inet_addr(server_ip);  // Convert string IP to network byte order


    // Fill in the packet type
    ack_packet[ack_packet_size++] = TRANSMISSION_SHA_PACKET_TYPE;

	// add transmission ID
	uint32_t transmission_id_network = htonl(transmission_id);
	memcpy(&ack_packet[ack_packet_size], &transmission_id_network, sizeof(uint32_t));
	ack_packet_size += sizeof(uint32_t);

    // Add the status (8 bits)
    ack_packet[ack_packet_size++] = status;

    // Add crc to the end of the packet (calculated from the rest of the packet)
    uint32_t crc = calculate_crc32(ack_packet, ack_packet_size);
    crc = htonl(crc);
    memcpy(&ack_packet[ack_packet_size], &crc, sizeof(uint32_t));
    ack_packet_size += sizeof(uint32_t);

    // Send the acknowledgment packet
    ssize_t sent_len = sendto(sockfd, ack_packet, ack_packet_size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (sent_len == SOCKET_ERROR) {
        fprintf(stderr, "Failed to send acknowledgment packet 0x03\n");
    } else {
        printf("Acknowledgment 0x03 sent to %s:%u\n", server_ip, server_port);
    }
}

void send_acknowledgment(SOCKET sockfd, const char *server_ip, uint16_t
server_port, uint8_t packet_type, bool status, uint32_t
corrupted_packet_index, uint32_t transmission_id) {
    struct sockaddr_in server_addr;
    uint8_t ack_packet[BUFFER_SIZE];
    int ack_packet_size = 0;

    // initialize the server address structure
	printf("transmission_id: %u\n", transmission_id);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);  // network byte order -> big endian
    server_addr.sin_addr.s_addr = inet_addr(server_ip);  // string IP to network byte order -> big endian

    // fill in the packet type
    ack_packet[ack_packet_size++] = TRANSMISSION_ACK_PACKET_TYPE;

	// add transmission ID
	uint32_t transmission_id_network = htonl(transmission_id);
	memcpy(&ack_packet[ack_packet_size], &transmission_id_network, sizeof(uint32_t));
	ack_packet_size += sizeof(uint32_t);

    // packet type, 8 bits
    ack_packet[ack_packet_size++] = packet_type;

    // status, 8 bits
    ack_packet[ack_packet_size++] = status ? 1 : 0;

    // Only include this if the packet type is 0x01 (data packet) 32 bits
    if (packet_type == TRANSMISSION_DATA_PACKET_TYPE) {
    	uint32_t transmission_id_network = htonl(corrupted_packet_index);
        memcpy(&ack_packet[ack_packet_size], &transmission_id_network, sizeof(uint32_t));
        ack_packet_size += sizeof(uint32_t);
    }

    // Add CRC to the end of the packet (calculated from the rest of the packet)
    uint32_t crc = calculate_crc32(ack_packet, ack_packet_size);
    crc = htonl(crc);  // Ensure CRC is in network byte order
    memcpy(&ack_packet[ack_packet_size], &crc, sizeof(uint32_t));
    ack_packet_size += sizeof(uint32_t);

    // Send the acknowledgment packet
    ssize_t sent_len = sendto(sockfd, ack_packet, ack_packet_size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (sent_len == SOCKET_ERROR) {
        fprintf(stderr, "Failed to send acknowledgment packet 0x04\n");
    } else {
        printf("Acknowledgment 0x04 sent to %s:%u\n", server_ip, server_port);
    }
}
