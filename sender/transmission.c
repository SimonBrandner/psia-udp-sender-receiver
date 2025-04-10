#include <arpa/inet.h>
#include <openssl/evp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "./connection.h"
#include "./packet.h"
#include "./transmission.h"
#include "./utils.h"

size_t count_unacknowledged_packets(transmission_t *transmission) {
	size_t unacknowledged_packet_count = 0;

	for (size_t i = 0; i < transmission->current_index; ++i) {
		if (transmission->packets[i].acknowledgement == NONE) {
			++unacknowledged_packet_count;
		}
	}

	return unacknowledged_packet_count;
}

bool transmission_loop(transmission_t *transmission, uint8_t *data_buffer) {
	if (count_unacknowledged_packets(transmission) >
		MAX_UNACKNOWLEDGED_PACKETS) {
		packet_t packet = receive_packet(transmission->connection);

		switch (packet.packet_type) {
		case ACKNOWLEDGEMENT_PACKET_TYPE:;
			acknowledgement_packet_content_t *packet_content =
				(acknowledgement_packet_content_t *)packet.content;
			if (packet_content->packet_type != TRANSMISSION_DATA_PACKET_TYPE) {
				fprintf(stderr, "Packet of unknown type!");
				exit(NON_RECOVERABLE_ERROR_CODE);
			}
			if (packet_content->status) {
				transmission->packets[packet_content->index].acknowledgement =
					POSITIVE;
			} else {
				transmission->packets[packet_content->index].acknowledgement =
					NEGATIVE;
			}
		default:
			fprintf(stderr, "Packet of unknown type!");
			exit(NON_RECOVERABLE_ERROR_CODE);
		}

		return false;
	}
	for (size_t i = 0; i < transmission->current_index; ++i) {
		if (transmission->packets[i].acknowledgement == NEGATIVE) {
			// Resend packet
			send_packet_data(transmission->connection,
							 transmission->packets[i].packet_data,
							 transmission->packets[i].packet_data_size);
		}
	}
	// Send new packet
	size_t data_size;
	if ((data_size =
			 fread(data_buffer, 1, MAX_DATA_SIZE, transmission->file)) <= 0) {
		if (count_unacknowledged_packets(transmission) > 0) {
			return false;
		}

		// No data to send, no negative acknowledgements, no unacknowledged
		// packets - we are done
		return true;
	}

	transmission->packets[transmission->current_index] =
		send_transmission_data_packet(
			transmission->connection, transmission->transmission_id,
			transmission->current_index, data_buffer, data_size);
	if (!EVP_DigestUpdate(transmission->md_context, data_buffer, data_size)) {
		fprintf(stderr, "Failed to update EVP digest!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}

	++transmission->current_index;

	return false;
}

transmission_t create_transmission(connection_t connection, char *file_path) {
	// Prepare SHA-256
	EVP_MD_CTX *md_context = EVP_MD_CTX_new();
	if (!md_context) {
		fprintf(stderr, "Failed to create EVP context!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}
	if (!EVP_DigestInit_ex(md_context, EVP_sha256(), NULL)) {
		fprintf(stderr, "Failed to init EVP digest!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}

	// Prepare file reading
	FILE *file = fopen(file_path, "rb");
	if (file == NULL) {
		fprintf(stderr, "Failed to open file!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}

	transmission_t transmission;
	transmission.current_index = 0;
	transmission.file = file;
	transmission.file_name = (char *)get_file_name(file_path);
	transmission.file_size = get_file_size(file_path);
	transmission.length = transmission.file_size / MAX_DATA_SIZE + 1;
	transmission.connection = connection;
	transmission.md_context = md_context;
	transmission.transmission_id = get_random_number();
	transmission.packets = malloc(sizeof(sent_packet_t) * transmission.length);
	if (transmission.packets == NULL) {
		fprintf(stderr, "Failed to allocate space for packets!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}

	return transmission;
}

void destroy_transmission(transmission_t transmission) {
	for (size_t i = 0; i < transmission.current_index; ++i) {
		free(transmission.packets[i].packet_data);
	}
	EVP_MD_CTX_free(transmission.md_context);
	if (fclose(transmission.file)) {
		fprintf(stderr, "Failed to close file!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}
}

void resend_until_success(transmission_t transmission, uint8_t packet_type,
						  sent_packet_t packet) {
	while (true) {
		acknowledgement_packet_content_t *content;
		while (true) {
			packet_t received_packet = receive_packet(transmission.connection);
			if (received_packet.transmission_id !=
					transmission.transmission_id ||
				received_packet.packet_type != ACKNOWLEDGEMENT_PACKET_TYPE) {
				continue;
			}
			content = received_packet.content;
			if (content->packet_type != packet_type) {
				continue;
			}
			break;
		}
		if (content->status) {
			break;
		}
		send_packet_data(transmission.connection, packet.packet_data,
						 packet.packet_data_size);
	}
}

void start_transmission(transmission_t transmission) {
	sent_packet_t packet = send_transmission_start_packet(
		transmission.connection, transmission.transmission_id,
		transmission.length, transmission.file_name);
	resend_until_success(transmission, TRANSMISSION_START_PACKET_TYPE, packet);
}

void transmit_data(transmission_t transmission) {
	uint8_t *data_buffer = malloc(sizeof(uint8_t) * MAX_DATA_SIZE);
	if (data_buffer == NULL) {
		fprintf(stderr, "Malloc failed!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}
	while (true) {
		if (transmission_loop(&transmission, data_buffer)) {
			break;
		}
	}
	free(data_buffer);
}

void end_transmission(transmission_t transmission) {
	// Finalise hash
	uint8_t hash[HASH_SIZE];
	unsigned int hash_size = HASH_SIZE;
	if (!EVP_DigestFinal_ex(transmission.md_context, hash, &hash_size)) {
		fprintf(stderr, "Failed to final EVP digest!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}

	sent_packet_t packet = send_transmission_end_packet(
		transmission.connection, transmission.transmission_id,
		transmission.file_size, hash);

	resend_until_success(transmission, TRANSMISSION_END_PACKET_TYPE, packet);
}

void transmit_file(connection_t connection, char *file_path) {
	transmission_t transmission = create_transmission(connection, file_path);

	start_transmission(transmission);
	transmit_data(transmission);
	end_transmission(transmission);

	destroy_transmission(transmission);
}
