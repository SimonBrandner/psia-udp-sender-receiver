#include <arpa/inet.h>
#include <openssl/evp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "./connection.h"
#include "./packet.h"
#include "./utils.h"

#define WAIT_TIME 10000000 // 10 ms
#define MAX_DATA_SIZE 1000 // 1 kB

void transmit_file(connection_t connection, char *file_path) {
	struct timespec wait_time_spec = {.tv_sec = 0, .tv_nsec = WAIT_TIME};

	const uint32_t transmission_id = get_random_number();
	const uint32_t file_size = get_file_size(file_path);
	const char *file_name = get_file_name(file_path);

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
	uint32_t index = 0;
	FILE *file = fopen(file_path, "rb");
	if (file == NULL) {
		fprintf(stderr, "Failed to open file!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}
	uint8_t *data = malloc(sizeof(uint8_t) * MAX_DATA_SIZE);
	if (data == NULL) {
		fprintf(stderr, "Malloc failed!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}

	// Start transmission
	send_transmission_start_packet(connection, transmission_id,
								   file_size / MAX_DATA_SIZE + 1, file_name);

	// Transmit data
	size_t data_size;
	while ((data_size = fread(data, 1, MAX_DATA_SIZE, file)) > 0) {
		send_transmission_data_packet(connection, transmission_id, index, data,
									  data_size);
		if (!EVP_DigestUpdate(md_context, data, data_size)) {
			fprintf(stderr, "Failed to update EVP digest!\n");
			exit(NON_RECOVERABLE_ERROR_CODE);
		}

		++index;
		nanosleep(&wait_time_spec, NULL);
	}
	free(data);
	if (fclose(file)) {
		fprintf(stderr, "Failed to close file!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}

	// Finalise hash
	uint8_t hash[HASH_SIZE];
	unsigned int hash_size = HASH_SIZE;
	if (!EVP_DigestFinal_ex(md_context, hash, &hash_size)) {
		fprintf(stderr, "Failed to final EVP digest!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}
	EVP_MD_CTX_free(md_context);

	// End transmission
	send_transmission_end_packet(connection, transmission_id, file_size, hash);
}

int main(int argc, char **argv) {
	// We are going to need this later for generating random
	// ids etc.
	srand(time(NULL));

	if (argc != 5) {
		fprintf(stderr, "Not enough arguments supplied - see --help!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}

	char *filename = argv[1];
	unsigned int source_port = atoi(argv[2]);
	char *target_ip_address = argv[3];
	unsigned int target_port = atoi(argv[4]);

	connection_t connection = create_connection(target_ip_address, target_port);

	transmit_file(connection, filename);

	close_connection(connection);

	return EXIT_SUCCESS;
}
