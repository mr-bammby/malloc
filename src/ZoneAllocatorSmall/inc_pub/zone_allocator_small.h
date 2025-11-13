#ifndef IG_ZONE_ALLOCATOR_SMALL_H
#define IG_ZONE_ALLOCATOR_SMALL_H

#include <stddef.h>

#define SMALL_ALLOC_SIZE_MAX 8192u // Size of the small allocation
#define SMALL_ALLOC_SIZE_MIN 64u // Minimum size of the small allocation

void *ZoneAllocatorSmall_alloc(size_t size);
short ZoneAllocatorSmall_free(void *ptr);
short ZoneAllocatorSmall_realloc(void **ptr, size_t size);

size_t ZoneAllocatorSmall_size_get(void *ptr);
void ZoneAllocatorSmall_report(void);


#endif // IG_ZONE_ALLOCATOR_SMALL_H