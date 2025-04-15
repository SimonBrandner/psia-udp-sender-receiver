#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <openssl/evp.h>

#include "packet.h"
#include "sender.h"
#include "receiver.h"

// Calculate SHA-256 hash from the data packets in the transmission structure, using non deprecated OpenSSL functions
void calculate_sha256_from_packets(transmission_t *trans, unsigned char *output_hash) {
    if (!trans || !trans->data_packets || trans->total_packet_count == 0) {
        fprintf(stderr, "Invalid transmission structure, or empty data\n");
        return;
    }

    // Initialize OpenSSL`
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (mdctx == NULL) {
        fprintf(stderr, "Failed to create EVP_MD_CTX\n");
        return;
    }

    // Initialize SHA-256 context
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
        fprintf(stderr, "EVP_DigestInit_ex failed\n");
        EVP_MD_CTX_free(mdctx);
        return;
    }

    // Process each data packet
    for (uint32_t i = 0; i < trans->total_packet_count; i++) {
        if (trans->data_packets[i] != NULL) {
            if (EVP_DigestUpdate(mdctx, trans->data_packets[i], trans->packet_sizes[i]) != 1) {
                fprintf(stderr, "EVP_DigestUpdate failed\n");
                EVP_MD_CTX_free(mdctx);
                return;
            }
        }
    }

    // Calculate the final hash and save into output_hash
    if (EVP_DigestFinal_ex(mdctx, output_hash, NULL) != 1) {
        fprintf(stderr, "EVP_DigestFinal_ex failed\n");
        EVP_MD_CTX_free(mdctx);
        return;
    }

    EVP_MD_CTX_free(mdctx);
}

int process_packet_start_0x00(uint8_t *buffer, transmission_t **trans) {
    if (*trans) {
        fprintf(stderr, "Error: Received start packet while another transmission is in progress.\n");
        return CONTINUE_TRANSMISSION;
    }

    // Init trans
    *trans = malloc(sizeof(transmission_t));
    if (!*trans) {
        fprintf(stderr, "Memory allocation failed\n");
        return STOP_TRANSMISSION;
    }

    // Save the transmission ID, total packet count, and file name
    memcpy(&(*trans)->transmission_id, &buffer[1], sizeof(uint32_t));
    memcpy(&(*trans)->total_packet_count, &buffer[5], sizeof(uint32_t));
    strncpy((*trans)->file_name, (char *)&buffer[9], sizeof((*trans)->file_name) - 1);
    (*trans)->file_name[sizeof((*trans)->file_name) - 1] = '\0';

    (*trans)->transmission_id = ntohl((*trans)->transmission_id);
    (*trans)->total_packet_count = ntohl((*trans)->total_packet_count);

    printf("Transmission Start: ID %u, Packets %u, File %s\n", (*trans)->transmission_id, (*trans)->total_packet_count, (*trans)->file_name);

    (*trans)->data_packets = calloc((*trans)->total_packet_count, sizeof(char *));
    (*trans)->packet_sizes = calloc((*trans)->total_packet_count, sizeof(size_t));
    (*trans)->file_size = 0;
    (*trans)->current_packet_count = 0;
    return CONTINUE_TRANSMISSION;
}

int process_packet_data_0x01(uint8_t *buffer, transmission_t **trans, ssize_t recv_len, uint32_t *packet_index_address) {
    if (!*trans) {
        fprintf(stderr, "Error: Received data packet before transmission start.\n");
        return CONTINUE_TRANSMISSION_NO_ACK;
    }

    transmission_t *t = *trans;  // cleaner to dereference once

    // Save the transmission ID and packet index
    uint32_t transmission_id, packet_index;
    memcpy(&transmission_id, &buffer[1], sizeof(uint32_t));
    memcpy(&packet_index, &buffer[5], sizeof(uint32_t));

    transmission_id = ntohl(transmission_id);
	transmission_id = 42;
    packet_index = ntohl(packet_index);
    *packet_index_address = packet_index;

    if (packet_index >= t->total_packet_count) {
        fprintf(stderr, "Error: Packet index %u out of bounds\n", packet_index);
        return CONTINUE_TRANSMISSION_NO_ACK;
    }

    if (t->data_packets[packet_index] != NULL) {
        printf("Packet %u already received, sending acknowledgment\n", packet_index);
        return CONTINUE_TRANSMISSION;
    }

    if (transmission_id != t->transmission_id) {
        fprintf(stderr, "Error: Transmission ID mismatch. Got %u, expected %u\n", transmission_id, t->transmission_id);
        return CONTINUE_TRANSMISSION_NO_ACK;
    }

    // Load the data
    size_t data_size = recv_len - 13;  // 1 byte type, 4 ID, 4 index, 4 CRC32
    t->data_packets[packet_index] = malloc(data_size);
    if (!t->data_packets[packet_index]) {
        fprintf(stderr, "Memory allocation failed for packet %u\n", packet_index);
        return STOP_TRANSMISSION;
    }

    memcpy(t->data_packets[packet_index], &buffer[9], data_size);
    t->packet_sizes[packet_index] = data_size;
    t->file_size += data_size;
    t->current_packet_count++;

    printf("PID: [%u] LEFT: %u TID: %u\n", packet_index, t->total_packet_count - t->current_packet_count, transmission_id);
    return CONTINUE_TRANSMISSION;
}

int process_packet_end_0x02(uint8_t *buffer, transmission_t **trans, boolean *file_saved) {
    if (*file_saved) {
        return STOP_TRANSMISSION_SUCCESS;
    }

    if (!*trans) {
        fprintf(stderr, "Error: Received end packet before transmission start.\n");
        return CONTINUE_TRANSMISSION_NO_ACK;
    }

    // Save the transmission ID and file size
    memcpy(&(*trans)->file_size, &buffer[5], sizeof(uint32_t));
    (*trans)->file_size = ntohl((*trans)->file_size);
    memcpy((*trans)->file_hash, &buffer[9], SHA256_DIGEST_LENGTH);

    printf("File hash received\n");
    printf("Transmission ID: %u\n", (*trans)->transmission_id);
    printf("File Size: %u\n", (*trans)->file_size);

    printf("End Packet: File Size: %u bytes, SHA-256: ", (*trans)->file_size);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        printf("%02x", (*trans)->file_hash[i]);
    }
    printf("\n");

    // Validate SHA
    unsigned char file_hash[SHA256_DIGEST_LENGTH];
    calculate_sha256_from_packets(*trans, file_hash);

    if (memcmp((*trans)->file_hash, file_hash, SHA256_DIGEST_LENGTH) != 0) {
        fprintf(stderr, "SHA-256 hash mismatch or packets missing\n");
        printf("Expected: ");
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            printf("%02x", (*trans)->file_hash[i]);
        }
        printf("\n");

        printf("Computed: ");
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            printf("%02x", file_hash[i]);
        }
        printf("\n");

        return SHA256_MISSMATCH;
    }

    FILE *file = fopen((*trans)->file_name, "wb");
    if (!file) {
        fprintf(stderr, "File creation failed\n");
        return STOP_TRANSMISSION;
    }

    for (uint32_t i = 0; i < (*trans)->total_packet_count; i++) {
        fwrite((*trans)->data_packets[i], 1, (*trans)->packet_sizes[i], file);
        free((*trans)->data_packets[i]);
    }

    *file_saved = true;
    printf("File has been written successfully\n");

    fclose(file);
    free((*trans)->data_packets);
    free((*trans)->packet_sizes);
    free(*trans);
    *trans = NULL;

    return STOP_TRANSMISSION_SUCCESS;
}
