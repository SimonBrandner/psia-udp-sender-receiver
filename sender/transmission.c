#include <arpa/inet.h>
#include <openssl/evp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "./connection.h"
#include "./main.h"
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

bool receive_acknowledgement_packet(transmission_t *transmission) {
	packet_t packet;
	if (receive_packet(transmission->connection, &packet)) {
		switch (packet.packet_type) {
		case ACKNOWLEDGEMENT_PACKET_TYPE:;
			acknowledgement_packet_content_t *packet_content =
				(acknowledgement_packet_content_t *)packet.content;
			if (packet_content->packet_type != TRANSMISSION_DATA_PACKET_TYPE) {
				return false;
			}
			if (packet_content->status) {
				transmission->packets[packet_content->index].acknowledgement =
					POSITIVE;
			} else if (packet_content->index < transmission->current_index) {
				transmission->packets[packet_content->index].acknowledgement =
					NEGATIVE;
			}
			return true;
			break;
		}
	}

	return false;
}

bool transmission_loop(transmission_t *transmission, uint8_t *data_buffer) {
	size_t unacknowledged_packets_count =
		count_unacknowledged_packets(transmission);
	bool end_of_file = feof(transmission->file);

	if (end_of_file && unacknowledged_packets_count == 0) {
		printf("All data successfully received by the receiver.\n");
		return true;
	}

	struct timeval now_struct;
	gettimeofday(&now_struct, NULL);
	uint64_t now = now_struct.tv_sec * 1000000 + now_struct.tv_usec;

	for (size_t i = 0; i < transmission->current_index; ++i) {
		sent_packet_t *sent_packet = &transmission->packets[i];
		if (sent_packet->acknowledgement != POSITIVE &&
			now - sent_packet->time_stamp > RESEND_TIMEOUT) {
			// Resend packet
			send_packet_data(transmission->connection, sent_packet->packet_data,
							 sent_packet->packet_data_size);
			sent_packet->time_stamp = now;
		}
	}
	if (unacknowledged_packets_count > MAX_UNACKNOWLEDGED_PACKETS ||
		end_of_file) {
		if (!receive_acknowledgement_packet(transmission)) {
			sleep_for_milliseconds(WAIT_TIME);
		}
		return false;
	}
	// Send new packet
	size_t data_size;
	if ((data_size =
			 fread(data_buffer, 1, MAX_DATA_SIZE, transmission->file)) <= 0) {
		printf("All of the data transmitted.\n");
		return false;
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
	// transmission.transmission_id = get_random_number();
	transmission.transmission_id = 42;
	transmission.packets = malloc(sizeof(sent_packet_t) * transmission.length);
	if (transmission.packets == NULL) {
		fprintf(stderr, "Failed to allocate space for packets!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}

	return transmission;
}

void destroy_transmission(transmission_t *transmission) {
	for (size_t i = 0; i < transmission->current_index; ++i) {
		free(transmission->packets[i].packet_data);
	}
	EVP_MD_CTX_free(transmission->md_context);
	if (fclose(transmission->file)) {
		fprintf(stderr, "Failed to close file!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}
}

bool resend_until_success_or_timeout(transmission_t *transmission,
									 uint8_t packet_type, sent_packet_t packet,
									 packet_t *haha, bool *hihi) {
	struct timeval outter_start;
	gettimeofday(&outter_start, NULL);
	struct timeval inner_start;
	while (true) {
		gettimeofday(&inner_start, NULL);
		acknowledgement_packet_content_t *content = NULL;
		printf(
			"Waiting for acknowledgement of start/end transmission packet.\n");
		while (true) {
			if (timeout_elapsed(&inner_start, 1)) {
				printf("Have not received an acknowledgement - resending.\n");
				break;
			}
			if (timeout_elapsed(&outter_start, TIMEOUT_SECONDS)) {
				printf(
					"Waiting for start/end transmission packet timed-out.\n");
				return false;
			}

			packet_t received_packet;
			if (!receive_packet(transmission->connection, &received_packet)) {
				sleep_for_milliseconds(WAIT_TIME);
				continue;
			}
			if (transmission->transmission_id ==
					received_packet.transmission_id &&
				received_packet.packet_type ==
					TRANSMISSION_END_RESPONSE_PACKET_TYPE) {
				*haha = received_packet;
				*hihi = true;
				printf("Received end of transmission before ack.\n");
				return true;
			}
			if (received_packet.transmission_id !=
					transmission->transmission_id ||
				received_packet.packet_type != ACKNOWLEDGEMENT_PACKET_TYPE) {
				printf("Transmission ID or packet type mismatch!\n");
				printf("Packet type: %x\n", received_packet.packet_type);
				printf("Transmission id: %x != %x\n",
					   received_packet.transmission_id,
					   transmission->transmission_id);
				sleep_for_milliseconds(WAIT_TIME);
				continue;
			}
			content = received_packet.content;
			if (content->packet_type != packet_type) {
				sleep_for_milliseconds(WAIT_TIME);
				continue;
			}

			break;
		}

		if (content) {
			if (content->status) {
				break;
			} else {
				printf("Received negative acknowledgement of start/end "
					   "transmission "
					   "packet.\n");
			}
			content = NULL;
		}

		send_packet_data(transmission->connection, packet.packet_data,
						 packet.packet_data_size);
	}
	return true;
}

bool start_transmission(transmission_t *transmission) {
	sent_packet_t packet = send_transmission_start_packet(
		transmission->connection, transmission->transmission_id,
		transmission->length, transmission->file_name);
	printf("Sent transmission start packet.\n");
	return resend_until_success_or_timeout(
		transmission, TRANSMISSION_START_PACKET_TYPE, packet, NULL, NULL);
}

bool transmit_data(transmission_t *transmission) {
	printf("Starting to transmit data.\n");
	uint8_t *data_buffer = malloc(sizeof(uint8_t) * MAX_DATA_SIZE);
	if (data_buffer == NULL) {
		fprintf(stderr, "Malloc failed!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}

	uint32_t last_index = 0;
	struct timeval last_index_update_time;
	gettimeofday(&last_index_update_time, NULL);
	while (true) {
		if (timeout_elapsed(&last_index_update_time, TIMEOUT_SECONDS)) {
			printf("Data transmission has failed - the receiver has not "
				   "answered in too long.\n");
			return false;
		}

		if (transmission_loop(transmission, data_buffer)) {
			break;
		}
		if (transmission->current_index != last_index) {
			last_index = transmission->current_index;
			gettimeofday(&last_index_update_time, NULL);
		}
	}
	free(data_buffer);
	return true;
}

bool end_transmission(transmission_t *transmission) {
	// Finalise hash
	uint8_t hash[HASH_SIZE];
	unsigned int hash_size = HASH_SIZE;
	if (!EVP_DigestFinal_ex(transmission->md_context, hash, &hash_size)) {
		fprintf(stderr, "Failed to final EVP digest!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}

	sent_packet_t packet = send_transmission_end_packet(
		transmission->connection, transmission->transmission_id,
		transmission->file_size, hash);

	packet_t received_packet;
	bool hihi = false;
	if (!resend_until_success_or_timeout(transmission,
										 TRANSMISSION_END_PACKET_TYPE, packet,
										 &received_packet, &hihi)) {
		return false;
	}
	if (hihi) {
		transmission_end_response_packet_content_t *content =
			received_packet.content;
		return content->status;
	}

	printf("Received acknowledgement of end of transmission packet.\n");

	struct timeval start;
	gettimeofday(&start, NULL);
	while (true) {
		if (timeout_elapsed(&start, TIMEOUT_SECONDS)) {
			printf(
				"We have not received a confirmation but the receiver is going "
				"to close their socket now, so there is not much we can do.\n");
			return true;
		}

		if (!receive_packet(transmission->connection, &received_packet)) {
			sleep_for_milliseconds(WAIT_TIME);
			continue;
		}
		if (received_packet.packet_type !=
			TRANSMISSION_END_RESPONSE_PACKET_TYPE) {
			sleep_for_milliseconds(WAIT_TIME);
			continue;
		}
		transmission_end_response_packet_content_t *content =
			received_packet.content;
		if (content->status) {
			printf("Transmission was successful.\n");
		} else {
			printf("Hash does not match - attempting to retransmit.\n");
		}
		return content->status;
	}
}

void transmit_file(connection_t connection, char *file_path) {
	while (true) {
		transmission_t transmission =
			create_transmission(connection, file_path);

		if (!start_transmission(&transmission)) {
			printf("We failed to start the transmission - there is not much we "
				   "can do.\n");
			destroy_transmission(&transmission);
			break;
		}
		if (!transmit_data(&transmission)) {
			break;
		}
		if (end_transmission(&transmission)) {
			destroy_transmission(&transmission);
			break;
		}

		destroy_transmission(&transmission);
	}
	printf("Transmission ended.\n");
}
