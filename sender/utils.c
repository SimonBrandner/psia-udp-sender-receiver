#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include "utils.h"

uint32_t get_random_number() { return (uint32_t)rand() << 16 | rand(); }

const char *get_file_name(const char *file_path) {
	const char *filename = strrchr(file_path, '/');
	if (filename) {
		return filename + 1;
	}
	return file_path;
}

uint32_t get_file_size(const char *file_path) {
	struct stat st;
	stat(file_path, &st);
	return st.st_size;
}

void sleep_for_milliseconds(uint32_t milliseconds) {
	struct timespec ts = {0, milliseconds * 1000000L}; // 500 ms
	nanosleep(&ts, NULL);
}

bool timeout_elapsed(struct timeval *start, int seconds) {
	struct timeval now;
	gettimeofday(&now, NULL);
	int elapsed = now.tv_sec - start->tv_sec;
	if (elapsed < 0) {
		printf("Something went awry with time.\n");
		exit(NON_RECOVERABLE_ERROR_CODE);
	}
	return elapsed >= seconds;
}
