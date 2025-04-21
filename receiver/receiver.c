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
#include "logger.h"

DWORD WINAPI spawn(LPVOID lpParam) {
    printf("Exiting program after 10 seconds.\n");
    Sleep(10000);
    ExitProcess(0);
    return 0;
}

int handle_packet(SOCKET sockfd, SOCKET clientfd, const char *sender_ip_address, uint16_t sender_port, transmission_t **trans, boolean *file_saved) {
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


	uint32_t transmission_id = 0;
    if (trans && *trans) {
       transmission_id = (*trans)->transmission_id;
    }

    // validate the CRC-32 checksum
    if (!check_packet_crc32(buffer, recv_len, sender_ip_address, sender_port, clientfd, packet_type, received_crc, transmission_id)) {
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
    	send_acknowledgment(clientfd, sender_ip_address, sender_port,
		packet_type, true, packet_index, transmission_id);
    	Sleep(100);
        result = process_packet_end_0x02(buffer, trans, file_saved);
    }

    if (result == CONTINUE_TRANSMISSION) {
        send_acknowledgment(clientfd, sender_ip_address, sender_port,
        packet_type, true, packet_index, transmission_id);
    } else if (result == SHA256_MISSMATCH) {
        fprintf(stderr, "SHA256 mismatch\n");
        send_sha256_acknowledgement(clientfd, sender_ip_address, sender_port,
         0, transmission_id);
    } else if (result == STOP_TRANSMISSION_SUCCESS) {
        send_sha256_acknowledgement(clientfd, sender_ip_address, sender_port,
         1, transmission_id);
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
	// Bind clientfd to port 53000
	struct sockaddr_in client_addr;
	memset(&client_addr, 0, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = INADDR_ANY;  // Use specific IP if needed
	client_addr.sin_port = htons(sender_port);

	if (bind(clientfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) == SOCKET_ERROR) {
		fprintf(stderr, "Failed to bind client socket to port 53000: %d\n", WSAGetLastError());
		closesocket(sockfd);
		closesocket(clientfd);
		WSACleanup();
		return false;
	}

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
    //server_addr.sin_addr.s_addr = inet_addr(sender_ip_address); // works for me
	server_addr.sin_addr.s_addr = INADDR_ANY; // works for communication with  others than me lol
	server_addr.sin_port = htons(receiver_port);

	if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
		fprintf(stderr, "Bind failed with error code: %d\n", WSAGetLastError());
		closesocket(sockfd);
		WSACleanup();
		return false;
	}

    printf("Listening on port %d...\n", receiver_port);

    boolean file_saved = false;
    boolean exiting = false;

    int result;
    transmission_t *trans = NULL;
    while (true) { // loop until transmission is complete
        result = handle_packet(sockfd, clientfd, sender_ip_address, sender_port, &trans, &file_saved);
        if (result == SHA256_MISSMATCH) {
            closesocket(sockfd);
            WSACleanup();
            return false;
        } else if (result == STOP_TRANSMISSION) {
            if (!exiting) {
                HANDLE thread = CreateThread(NULL, 0, spawn, NULL, 0, NULL);
                exiting = true;
            }
            continue; // break; if no need to wait asynchronously for 10 seconds
        } else if (result == CONTINUE_TRANSMISSION) {
            continue;
        }
    }

    closesocket(sockfd);
    WSACleanup();
    return true;
}
