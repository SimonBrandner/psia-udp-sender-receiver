#ifndef TRANSMISSION_H
#define TRANSMISSION_H

#include "./connection.h"
#include <openssl/evp.h>

#define MAX_DATA_SIZE 1000 // 1 kB
#define MAX_UNACKNOWLEDGED_PACKETS 10

typedef struct {
	sent_packet_t *packets;
	size_t length;
	connection_t connection;
	FILE *file;
	size_t file_size;
	char *file_name;
	EVP_MD_CTX *md_context;
	size_t current_index;
	uint32_t transmission_id;
} transmission_t;

void transmit_file(connection_t connection, char *file_path);

#endif // TRANSMISSION_H
