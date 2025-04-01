#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
