#ifndef IG_ZONE_ALLOCATOR_LARGE_H
#define IG_ZONE_ALLOCATOR_LARGE_H

#include <stddef.h>

#define LARGE_ALLOC_SIZE_MIN 8192u // Minimum size of the big allocation

void *ZoneAllocatorLarge_alloc(size_t size);
short ZoneAllocatorLarge_free(void *ptr);
short ZoneAllocatorLarge_realloc(void **ptr, size_t size);

size_t ZoneAllocatorLarge_size_get(void *ptr);
void ZoneAllocatorLarge_report(void);
void ZoneAllocatorLarge_dump(void);



#endif // IG_ZONE_ALLOCATOR_LARGE_H