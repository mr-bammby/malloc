#ifndef IG_ZONE_ALLOCATOR_TINY_H
#define IG_ZONE_ALLOCATOR_TINY_H

#include <stdlib.h>
#include <inttypes.h>

#define TINY_ALLOC_SIZE 64u // Size of each allocation For best performance multiple of 16

void *ZoneAllocatorTiny_alloc(size_t size);
short ZoneAllocatorTiny_free(void *ptr);
void *ZoneAllocatorTiny_realloc(void *ptr, size_t size);

uint8_t ZoneAllocatorTiny_size_get(void *ptr);
void ZoneAllocatorTiny_report(void);



#endif // IG_ZONE_ALLOCATOR_TINY_H  