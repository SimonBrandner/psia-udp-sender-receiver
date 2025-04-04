#ifndef PACKET_H
#define PACKET_H

#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TRANSMISSION_START_PACKET_TYPE 0x0
#define TRANSMISSION_DATA_PACKET_TYPE 0x1
#define TRANSMISSION_END_PACKET_TYPE 0x2
#define CRC_SIZE 4
#define HASH_SIZE 32

typedef struct packet_t {
	uint8_t packet_type;
	uint32_t transmission_id;
	void *content;
} packet_t;

typedef struct transmission_start_packet_content_t {
	uint32_t transmission_length;
	const char *file_name;
} transmission_start_packet_content_t;

typedef struct transmission_data_packet_content_t {
	uint32_t index;
	uint32_t data_size;
	uint8_t *data;
} transmission_data_packet_content_t;

typedef struct transmission_end_packet_content_t {
	uint32_t file_size;
	uint8_t hash[HASH_SIZE];
} transmission_end_packet_content_t;

void serialize_transmission_data_packet_content(
	transmission_data_packet_content_t *packet_content,
	uint8_t **packet_content_data, size_t *packet_content_size);

void serialize_transmission_start_packet_content(
	transmission_start_packet_content_t *packet_content,
	uint8_t **packet_content_data, size_t *packet_content_size);

void serialize_transmission_end_packet_content(
	transmission_end_packet_content_t *packet_content,
	uint8_t **packet_content_data, size_t *packet_content_size);

void serialize_packet(packet_t *packet, uint8_t **packet_data,
					  size_t *packet_size);

#endif // PACKET_H
