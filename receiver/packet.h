#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <openssl/sha.h>
#include <winsock2.h>

#define TRANSMISSION_START_PACKET_TYPE 0x00 // Packet type for transmission start
#define TRANSMISSION_DATA_PACKET_TYPE 0x01  // Packet type for data
#define TRANSMISSION_END_PACKET_TYPE 0x02   // Packet type for transmission end
#define TRANSMISSION_SHA_PACKET_TYPE 0x03   // Packet type for acknowledgment
#define TRANSMISSION_ACK_PACKET_TYPE 0x04   // Packet type for error

typedef struct {
    uint32_t transmission_id;    // Unique ID for the transmission
    uint32_t total_packet_count; // Total number of packets expected
    size_t *packet_sizes;        // Array to hold sizes of each packet
    char file_name[1024];        // Name of the file being transmitted
    char **data_packets;         // Array of pointers to hold the data packets
    int current_packet_count;    // Current number of packets received
    uint32_t file_size;          // Size of the file being transmitted
    unsigned char file_hash[SHA256_DIGEST_LENGTH]; // SHA-256 hash of the file
} transmission_t;

// Function to calculate SHA-256 hash from the data packets in the transmission structure
void calculate_sha256_from_packets(transmission_t *trans, unsigned char *output_hash);

// Function to process the start packet (0x00) and initialize the transmission structure
int process_packet_start_0x00(uint8_t *buffer, transmission_t **trans);

// Function to process the data packet (0x01) and store the data in the transmission structure
int process_packet_data_0x01(uint8_t *buffer, transmission_t **trans, ssize_t recv_len, uint32_t *packet_index_address);

// Function to process the end packet (0x02) and finalize the transmission structure
int process_packet_end_0x02(uint8_t *buffer, transmission_t **trans);

#endif //PACKET_H
