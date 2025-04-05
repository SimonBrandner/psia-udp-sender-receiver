#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <stdbool.h>
#include "receiver.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Not enough arguments given!\n");
        fprintf(stderr, "Usage: %s <receiver_port> <target_ip_address> <sender_port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int receiver_port = atoi(argv[1]);
    char *sender_ip_address = argv[2];
    int sender_port = atoi(argv[3]);

    printf("Receiver Port: %d\n", receiver_port);
    printf("Sender IP Address: %s\n", sender_ip_address);
    printf("Sender Port: %d\n\n", sender_port);

    // Loop until new transmission is successful
    while (new_transmission(receiver_port, sender_ip_address, sender_port) == false) {
        fprintf(stderr, "Failed to receive transmission. Retrying...\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
