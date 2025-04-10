#include <arpa/inet.h>
#include <openssl/evp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "./connection.h"
#include "./transmission.h"
#include "./utils.h"

int main(int argc, char **argv) {
	// We are going to need this later for generating random
	// ids etc.
	srand(time(NULL));

	if (argc != 5) {
		fprintf(stderr, "Not enough arguments supplied - see --help!\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}

	char *filename = argv[1];
	unsigned int receiver_port = atoi(argv[2]);
	char *receiver_ip_address = argv[3];
	unsigned int sender_port = atoi(argv[4]);

	connection_t connection =
		create_connection(receiver_ip_address, receiver_port, sender_port);

	transmit_file(connection, filename);

	close_connection(connection);

	return EXIT_SUCCESS;
}
