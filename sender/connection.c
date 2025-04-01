#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "netinet/in.h"

#include "./connection.h"
#include "./utils.h"

int create_socket() {
	int socket_file_descriptor;
	if ((socket_file_descriptor = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "Socket creation failed");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}
	return socket_file_descriptor;
}

struct sockaddr_in create_receiver_address(char *target_ip_address,
										   unsigned int target_port) {
	struct sockaddr_in receiver_address;
	receiver_address.sin_family = AF_INET;
	receiver_address.sin_port = htons(target_port);
	inet_pton(AF_INET, target_ip_address, &receiver_address.sin_addr);

	return receiver_address;
}

connection_t create_connection(char *target_ip_address,
							   unsigned int target_port) {
	connection_t connection;
	connection.socket_file_descriptor = create_socket();
	connection.receiver_address =
		create_receiver_address(target_ip_address, target_port);
	return connection;
}

void close_connection(connection_t connection) {
	close(connection.socket_file_descriptor);
}

void send_packet(connection_t connection, packet_t *packet) {
	uint8_t *packet_data = NULL;
	size_t packet_size;
	serialize_packet(packet, &packet_data, &packet_size);

	if (sendto(connection.socket_file_descriptor, packet_data, packet_size, 0,
			   (struct sockaddr *)&connection.receiver_address,
			   sizeof(connection.receiver_address)) < 0) {
		fprintf(stderr, "Failed to send packet!");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}
	free(packet_data);
}

void send_transmission_start_packet(connection_t connection,
									uint32_t transmission_id,
									uint32_t transmission_length,
									uint32_t file_size, const char *file_name) {
	transmission_start_packet_content_t content;
	content.transmission_length = transmission_length;
	content.file_size = file_size;
	content.file_name = file_name;

	packet_t packet;
	packet.packet_type = TRANSMISSION_START_PACKET_TYPE;
	packet.transmission_id = transmission_id;
	packet.content = &content;

	send_packet(connection, &packet);
}

void send_transmission_data_packet(connection_t connection,
								   uint32_t transmission_id, uint32_t index,
								   uint8_t *data, size_t data_size) {
	transmission_data_packet_content_t content;
	content.index = index;
	content.data = data;
	content.data_size = data_size;

	packet_t packet;
	packet.packet_type = TRANSMISSION_DATA_PACKET_TYPE;
	packet.transmission_id = transmission_id;
	packet.content = &content;

	send_packet(connection, &packet);
}
