#ifndef IG_MALLOC_H
#define IG_MALLOC_H

#include <stddef.h>

// Function declarations and public interfaces for malloc module

void *malloc(size_t size);
void *realloc(void* ptr, size_t size);
void free(void* ptr);
void show_alloc_mem(void);
void show_alloc_mem_ex(void);

#endif // IG_MALLOC_H