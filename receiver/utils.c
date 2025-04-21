#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <winsock2.h>

#include "utils.h"
#include "packet.h"
#include "receiver.h"
#include "sender.h"
#include "logger.h"

// Function to calculate CRC-32 using polynomial 0xEDB88320
uint32_t calculate_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFF;
}

// Function to validate received data against expected CRC-32
bool validate_crc32(const uint8_t *data, size_t length, uint32_t expected_crc) {
    uint32_t computed_crc = calculate_crc32(data, length);
    return computed_crc == expected_crc;
}

// Function to send acknowledgment packet if crc32 validation fails
bool check_packet_crc32(uint8_t *buffer, size_t recv_len, const char *sender_ip_address,
    uint16_t sender_port, SOCKET clientfd, uint8_t packet_type, uint32_t
    received_crc, uint32_t transmission_id) {

    if (!validate_crc32(buffer, recv_len - CRC32_LEN, received_crc)) {
        if (packet_type == TRANSMISSION_START_PACKET_TYPE) {
            send_acknowledgment(clientfd, sender_ip_address, sender_port, packet_type, false, DEFAULT_PACKET_INDEX, transmission_id);
        } else if (packet_type == TRANSMISSION_DATA_PACKET_TYPE) {
            uint32_t packet_index;
            memcpy(&packet_index, &buffer[5], sizeof(uint32_t));
            packet_index = ntohl(packet_index);
            send_acknowledgment(clientfd, sender_ip_address, sender_port, packet_type, false, packet_index, transmission_id);
        } else {
            send_acknowledgment(clientfd, sender_ip_address, sender_port, packet_type, false, DEFAULT_PACKET_INDEX, transmission_id);
        }
        return false;
    }
    return true;
}
