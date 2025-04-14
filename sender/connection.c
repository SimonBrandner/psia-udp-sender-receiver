#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "netinet/in.h"
#include "sys/socket.h"
#include "zlib.h"

#include "./connection.h"
#include "./packet.h"
#include "./utils.h"

void set_non_blocking(int sockfd) {
	int flags = fcntl(sockfd, F_GETFL, 0);
	if (flags == -1) {
		fprintf(stderr, "Failed to get socket flags!");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}

	if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
		fprintf(stderr, "Failed to set socket as non-blocking!");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}
}

int create_socket() {
	int socket_file_descriptor;
	if ((socket_file_descriptor = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "Socket creation failed");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}
	set_non_blocking(socket_file_descriptor);

	return socket_file_descriptor;
}

struct sockaddr_in create_receiver_address(char *receiver_ip_address,
										   unsigned int receiver_port) {
	struct sockaddr_in receiver_address;

	receiver_address.sin_family = AF_INET;
	receiver_address.sin_port = htons(receiver_port);
	inet_pton(AF_INET, receiver_ip_address, &receiver_address.sin_addr);

	return receiver_address;
}

struct sockaddr_in create_sender_address(unsigned int sender_port) {
	struct sockaddr_in sender_address;

	sender_address.sin_family = AF_INET;		  // IPv4
	sender_address.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
	sender_address.sin_port = htons(sender_port); // Port to listen on

	return sender_address;
}

connection_t create_connection(char *receiver_ip_address,
							   unsigned int receiver_port,
							   unsigned int sender_port) {
	connection_t connection;

	connection.sender_socket_file_descriptor = create_socket();
	connection.receiver_address =
		create_receiver_address(receiver_ip_address, receiver_port);

	connection.receiver_socket_file_descriptor = create_socket();
	connection.sender_address = create_sender_address(sender_port);

	return connection;
}

void close_connection(connection_t connection) {
	close(connection.sender_socket_file_descriptor);
}

void send_packet_data(connection_t connection, uint8_t *packet_data,
					  size_t packet_size) {
	if (sendto(connection.sender_socket_file_descriptor, packet_data,
			   packet_size, 0, (struct sockaddr *)&connection.receiver_address,
			   sizeof(connection.receiver_address)) < 0) {
		fprintf(stderr, "Failed to send packet!");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}
}

sent_packet_t send_packet(connection_t connection, packet_t *packet) {
	uint8_t *packet_data = NULL;
	size_t packet_size;
	serialize_packet(packet, &packet_data, &packet_size);

	send_packet_data(connection, packet_data, packet_size);

	sent_packet_t sent_packet;
	sent_packet.acknowledgement = NONE;
	sent_packet.packet_data = packet_data;
	sent_packet.packet_data_size = packet_size;
	return sent_packet;
}

sent_packet_t send_transmission_start_packet(connection_t connection,
											 uint32_t transmission_id,
											 uint32_t transmission_length,
											 const char *file_name) {
	transmission_start_packet_content_t content;
	content.transmission_length = transmission_length;
	content.file_name = file_name;

	packet_t packet;
	packet.packet_type = TRANSMISSION_START_PACKET_TYPE;
	packet.transmission_id = transmission_id;
	packet.content = &content;

	return send_packet(connection, &packet);
}

sent_packet_t send_transmission_data_packet(connection_t connection,
											uint32_t transmission_id,
											uint32_t index, uint8_t *data,
											size_t data_size) {
	transmission_data_packet_content_t content;
	content.index = index;
	content.data = data;
	content.data_size = data_size;

	packet_t packet;
	packet.packet_type = TRANSMISSION_DATA_PACKET_TYPE;
	packet.transmission_id = transmission_id;
	packet.content = &content;

	return send_packet(connection, &packet);
}

sent_packet_t send_transmission_end_packet(connection_t connection,
										   uint32_t transmission_id,
										   uint32_t file_size,
										   uint8_t hash[HASH_SIZE]) {
	transmission_end_packet_content_t content;
	content.file_size = file_size;
	memcpy(&content.hash, hash, HASH_SIZE);

	packet_t packet;
	packet.packet_type = TRANSMISSION_END_PACKET_TYPE;
	packet.transmission_id = transmission_id;
	packet.content = &content;

	return send_packet(connection, &packet);
}

bool receive_packet(connection_t connection, packet_t *packet) {
	uint8_t *packet_buffer = malloc(sizeof(uint8_t) * MAX_PACKET_SIZE);
	if (packet_buffer == NULL) {
		printf("Malloc failed in receive_packet()\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}

	socklen_t address_size = sizeof(connection.receiver_address);
	int packet_buffer_length = recvfrom(
		connection.receiver_socket_file_descriptor, (char *)packet_buffer,
		MAX_PACKET_SIZE, 0, (struct sockaddr *)&connection.receiver_address,
		&address_size);
	if (packet_buffer_length < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			// No data received
			return false;
		}

		fprintf(stderr, "Recvfrom failed!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}

	uint32_t received_crc;
	memcpy(&received_crc, packet_buffer + packet_buffer_length - CRC_SIZE,
		   CRC_SIZE);

	uint32_t calculated_crc = crc32(0L, Z_NULL, 0);
	calculated_crc = crc32(calculated_crc, (const Bytef *)packet_buffer,
						   packet_buffer_length - CRC_SIZE);
	calculated_crc = htonl(calculated_crc);
	if (received_crc != calculated_crc) {
		fprintf(stderr, "Received faulty packet - ignoring!\n");
		return false;
	}

	*packet = parse_packet(packet_buffer, packet_buffer_length);
	// Data received and parsed
	return true;
}
