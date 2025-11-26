#ifndef IG_MALLOC_PRIV_H
#define IG_MALLOC_PRIV_H

#include <stddef.h>

void *no_block_malloc(size_t size);
void no_block_free(void* ptr);

#endif // IG_MALLOC_PRIV_H