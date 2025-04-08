#ifndef RECEIVER_H
#define RECEIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <winsock2.h>

#include "packet.h"

#define CONTINUE_TRANSMISSION_NO_ACK -1
#define CONTINUE_TRANSMISSION 0
#define STOP_TRANSMISSION 1
#define SHA256_MISSMATCH 2
#define STOP_TRANSMISSION_SUCCESS 3
#define DEFAULT_PACKET_INDEX 0
#define BUFFER_SIZE 65507

#define MIN_RECEIVE_LEN 4
#define CRC32_LEN 4

// Function that handles the packet processing into transmission_t structure
int handle_packet(SOCKET sockfd, SOCKET clientfd, const char *sender_ip_address, uint16_t sender_port, transmission_t **trans, boolean *file_saved);

// Function that handles the new transmission
bool new_transmission();

#endif //RECEIVER_H
