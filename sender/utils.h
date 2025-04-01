#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

#define NON_RECOVERABLE_ERROR_CODE -1

uint32_t get_random_number();
const char *get_file_name(const char *filepath);
uint32_t get_file_size(const char *file_path);

#endif // UTILS_H
