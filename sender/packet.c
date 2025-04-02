#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "./packet.h"
#include "utils.h"

void serialize_transmission_start_packet_content(
	transmission_start_packet_content_t *packet_content,
	uint8_t **packet_content_data, size_t *packet_content_size) {
	// Calculate packet size
	*packet_content_size = sizeof(packet_content->transmission_length) +
						   strlen(packet_content->file_name) + 1;

	// Allocate space
	*packet_content_data = malloc(*packet_content_size);
	if (packet_content_data == NULL) {
		fprintf(stderr, "Malloc failed!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}
	uint8_t *packet_content_data_pointer = *packet_content_data;

	// Serialize data
	uint32_t transmission_length_net =
		htonl(packet_content->transmission_length);
	memcpy(packet_content_data_pointer, &transmission_length_net,
		   sizeof(transmission_length_net));
	packet_content_data_pointer += sizeof(transmission_length_net);

	memcpy(packet_content_data_pointer, packet_content->file_name,
		   strlen(packet_content->file_name) + 1);
}

void serialize_transmission_data_packet_content(
	transmission_data_packet_content_t *packet_content,
	uint8_t **packet_content_data, size_t *packet_content_size) {
	// Calculate packet size
	*packet_content_size =
		sizeof(packet_content->index) + packet_content->data_size;

	// Allocate space
	*packet_content_data = malloc(*packet_content_size);
	if (packet_content_data == NULL) {
		fprintf(stderr, "Malloc failed!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}
	uint8_t *packet_content_data_pointer = *packet_content_data;

	// Serialize
	uint32_t index_net = htonl(packet_content->index);
	memcpy(packet_content_data_pointer, &index_net, sizeof(index_net));
	packet_content_data_pointer += sizeof(index_net);

	memcpy(packet_content_data_pointer, packet_content->data,
		   packet_content->data_size);
}

void serialize_packet(packet_t *packet, uint8_t **packet_data,
					  size_t *packet_size) {
	// Serialize content
	uint8_t *packet_content_data = NULL;
	size_t packet_content_size;
	switch (packet->packet_type) {
	case TRANSMISSION_START_PACKET_TYPE:
		serialize_transmission_start_packet_content(
			(transmission_start_packet_content_t *)packet->content,
			&packet_content_data, &packet_content_size);
		break;
	case TRANSMISSION_DATA_PACKET_TYPE:
		serialize_transmission_data_packet_content(
			(transmission_data_packet_content_t *)packet->content,
			&packet_content_data, &packet_content_size);
		break;
	default:
		fprintf(stderr, "Packet of unknown type!");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}

	// Allocate space
	*packet_size = sizeof(packet->packet_type) +
				   sizeof(packet->transmission_id) + packet_content_size;
	*packet_data = malloc(*packet_size);
	if (packet_data == NULL) {
		fprintf(stderr, "Malloc failed!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}
	uint8_t *packet_data_pointer = *packet_data;

	// Serialize packet
	memcpy(packet_data_pointer, &packet->packet_type,
		   sizeof(packet->packet_type));
	packet_data_pointer += sizeof(packet->packet_type);

	uint32_t transmission_id_net = htonl(packet->transmission_id);
	memcpy(packet_data_pointer, &transmission_id_net,
		   sizeof(transmission_id_net));
	packet_data_pointer += sizeof(transmission_id_net);

	memcpy(packet_data_pointer, packet_content_data, packet_content_size);

	free(packet_content_data);
}
