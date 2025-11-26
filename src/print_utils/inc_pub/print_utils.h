#ifndef IG_PRINT_UTILS_H_
#define IG_PRINT_UTILS_H_

#include <stddef.h>
#include <unistd.h>

char *itoa(size_t num, int base, char *buffer);
char *itoa_size(size_t num, int base, int size, char *buffer);
void print_address_as_hex(void *ptr);
void print_size(size_t size);
void print_dump(void *ptr, size_t n);
void print_dump_header(size_t n);


void ft_memcpy(void * dest, void *src, size_t len);

#endif /* IG_PRINT_UTILS_H_ */
