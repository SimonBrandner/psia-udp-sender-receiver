#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "./connection.h"
#include "./utils.h"

#define WAIT_TIME 10000000 // 10 ms
#define MAX_DATA_SIZE 1000 // 1 kB

void transmit_file(connection_t connection, char *file_path) {
	struct timespec wait_time_spec = {.tv_sec = 0, .tv_nsec = WAIT_TIME};

	const uint32_t transmission_id = get_random_number();
	const uint32_t file_size = get_file_size(file_path);
	const char *file_name = get_file_name(file_path);

	send_transmission_start_packet(connection, transmission_id,
								   file_size / MAX_DATA_SIZE + 1, file_size,
								   file_name);

	uint32_t index = 0;
	FILE *file = fopen(file_path, "rb");
	uint8_t *data = malloc(sizeof(uint8_t) * MAX_DATA_SIZE);
	while (!feof(file)) {
		size_t data_size = fread(data, 1, MAX_DATA_SIZE, file);
		send_transmission_data_packet(connection, transmission_id, index, data,
									  data_size);

		++index;
		nanosleep(&wait_time_spec, NULL);
	}
	free(data);
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
