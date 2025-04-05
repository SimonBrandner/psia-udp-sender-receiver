#ifndef SENDER_H
#define SENDER_H

#include <stdint.h>
#include <stdbool.h>
#include <winsock2.h>

// Sends an acknowledgment packet to the given server IP and port.
void send_acknowledgment(SOCKET sockfd, const char *server_ip, uint16_t server_port, uint8_t packet_type,
                         bool status, uint32_t corrupted_packet_index);

// Calculates the SHA-256 hash of the data packets in the transmission structure.
void send_sha256_acknowledgement(SOCKET sockfd, const char *server_ip, uint16_t server_port, uint8_t status);

#endif //SENDER_H
