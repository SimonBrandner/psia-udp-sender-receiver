#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <winsock2.h>
#include <windows.h>
#include <openssl/sha.h>

#include "packet.h"
#include "sender.h"
#include "receiver.h"
#include "utils.h"

int handle_packet(SOCKET sockfd, SOCKET clientfd, const char *sender_ip_address, uint16_t sender_port, transmission_t **trans) {
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    uint8_t buffer[BUFFER_SIZE];

    // Receive the packet
    ssize_t recv_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
    if (recv_len < MIN_RECEIVE_LEN) {
        fprintf(stderr, "Received Packed corrupted/failed\n");
        return CONTINUE_TRANSMISSION;
    }

    uint8_t packet_type = buffer[0];
    uint32_t received_crc;
    memcpy(&received_crc, &buffer[recv_len - CRC32_LEN], 4);
    received_crc = ntohl(received_crc);

    // validate the CRC-32 checksum
    if (!check_packet_crc32(buffer, recv_len, sender_ip_address, sender_port, clientfd, packet_type, received_crc)) {
        fprintf(stderr, "CRC-32 validation failed for packet type %u\n", packet_type);
        return CONTINUE_TRANSMISSION;
    }

    // Process received the packet based on its type
    uint32_t packet_index = 0;
    int result = CONTINUE_TRANSMISSION_NO_ACK; // ignore other packet types, if not handled

    if (packet_type == TRANSMISSION_START_PACKET_TYPE) {
        result = process_packet_start_0x00(buffer, trans);

    } else if (packet_type == TRANSMISSION_DATA_PACKET_TYPE) {
        result = process_packet_data_0x01(buffer, trans, recv_len, &packet_index);

    } else if (packet_type == TRANSMISSION_END_PACKET_TYPE) {
        result = process_packet_end_0x02(buffer, trans);
    }

    if (result == CONTINUE_TRANSMISSION) {
        send_acknowledgment(clientfd, sender_ip_address, sender_port, packet_type, true, packet_index);
    } else if (result == SHA256_MISSMATCH) {
        fprintf(stderr, "SHA256 mismatch\n");
        send_sha256_acknowledgement(clientfd, sender_ip_address, sender_port, SHA256_MISSMATCH);
    } else if (result == STOP_TRANSMISSION_SUCCESS) {
        send_sha256_acknowledgement(clientfd, sender_ip_address, sender_port, STOP_TRANSMISSION_SUCCESS);
        result = STOP_TRANSMISSION;
    } else if (result == CONTINUE_TRANSMISSION_NO_ACK) {
        result = CONTINUE_TRANSMISSION;
    }
    return result;
}

bool new_transmission(unsigned int receiver_port, char *sender_ip_address, unsigned int sender_port) {
    WSADATA wsa;
    SOCKET sockfd;
    SOCKET clientfd;
    struct sockaddr_in server_addr;

    // set up the socket
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return false;
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    clientfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd == INVALID_SOCKET) {
        fprintf(stderr, "Socket creation failed\n");
        WSACleanup();
        return false;
    }

    if (clientfd == INVALID_SOCKET) {
        fprintf(stderr, "Client socket creation failed\n");
        closesocket(sockfd);
        WSACleanup();
        return false;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(sender_ip_address);
    server_addr.sin_port = htons(receiver_port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "Bind failed\n");
        closesocket(sockfd);
        WSACleanup();
        return false;
    }

    printf("Listening on port %d...\n", receiver_port);

    int result;
    transmission_t *trans = NULL;
    while (true) { // loop until transmission is complete
        result = handle_packet(sockfd, clientfd, sender_ip_address, sender_port, &trans);
        if (result == SHA256_MISSMATCH) {
            closesocket(sockfd);
            WSACleanup();
            return false;
        } else if (result == STOP_TRANSMISSION) {
            break;
        } else if (result == CONTINUE_TRANSMISSION) {
            continue;
        }
    }

    closesocket(sockfd);
    WSACleanup();
    return true;
}
