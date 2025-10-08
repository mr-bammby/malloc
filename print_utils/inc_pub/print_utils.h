#include <inttypes.h>
#include <stddef.h>
#include <unistd.h>

char *itoa(size_t num, int base, char *buffer);
char *itoa_size(size_t num, int base, int size, char *buffer);
void print_address_as_hex(void *ptr);
void print_size(size_t size);