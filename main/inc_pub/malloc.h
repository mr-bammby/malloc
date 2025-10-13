#ifndef MALLOC_H
#define MALLOC_H

#include <stddef.h>

// Function declarations and public interfaces for malloc module

void *ft_malloc(size_t size);
void *ft_realloc(void* ptr, size_t size);
void ft_free(void* ptr);


#endif // MALLOC_H