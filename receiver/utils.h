#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdbool.h>

// Function to calculate CRC32 checksum
uint32_t calculate_crc32(const uint8_t *data, size_t length);

// Function to validate CRC32 checksum
bool validate_crc32(const uint8_t *data, size_t length, uint32_t expected_crc);

// Function to send acknowledgment packet if CRC32 validation fails
bool check_packet_crc32(uint8_t *buffer, size_t recv_len, const char *sender_ip_address,
    uint16_t sender_port, SOCKET clientfd, uint8_t packet_type, uint32_t received_crc);

#endif //UTILS_H
