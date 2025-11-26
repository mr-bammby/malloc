#ifndef IG_ZONE_ALLOCATOR_TINY_H
#define IG_ZONE_ALLOCATOR_TINY_H

#include <stddef.h>
#include <inttypes.h>

#define TINY_ALLOC_SIZE (size_t)64u //Must be under 256

void *ZoneAllocatorTiny_alloc(size_t size);
short ZoneAllocatorTiny_free(void *ptr);
short ZoneAllocatorTiny_realloc(void **ptr, size_t size);

size_t ZoneAllocatorTiny_size_get(void *ptr);
void ZoneAllocatorTiny_report(void);

void ZoneAllocatorTiny_dump(void);

#endif // IG_ZONE_ALLOCATOR_TINY_H  