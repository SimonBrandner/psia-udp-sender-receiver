#ifndef CONNECTION_H
#define CONNECTION_H

#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "netinet/in.h"

#include "./packet.h"
#include "./utils.h"

typedef struct connection_t {
	struct sockaddr_in sender_address;
	struct sockaddr_in receiver_address;
	int socket;
} connection_t;

int create_socket();

struct sockaddr_in create_receiver_address(char *target_ip_address,
										   unsigned int target_port);

connection_t create_connection(char *receiver_ip_address,
							   unsigned int receiver_port,
							   unsigned int sender_port);

void close_connection(connection_t connection);

void send_packet_data(connection_t connection, uint8_t *packet_data,
					  size_t packet_size);

sent_packet_t send_packet(connection_t connection, packet_t *packet);

sent_packet_t send_transmission_start_packet(connection_t connection,
											 uint32_t transmission_id,
											 uint32_t transmission_length,
											 const char *file_name);

sent_packet_t send_transmission_data_packet(connection_t connection,
											uint32_t transmission_id,
											uint32_t index, uint8_t *data,
											size_t data_size);

sent_packet_t send_transmission_end_packet(connection_t connection,
										   uint32_t transmission_id,
										   uint32_t file_size,
										   uint8_t hash[HASH_SIZE]);

bool receive_packet(connection_t connection, packet_t *packet);

#endif // CONNECTION_H
