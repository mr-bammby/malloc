#ifndef IG_ZONE_ALLOCATOR_BIG_H
#define IG_ZONE_ALLOCATOR_BIG_H

#include <stdlib.h>

#define BIG_ALLOC_SIZE_MIN 4066u // Minimum size of the big allocation

void *ZoneAllocatorBig_alloc(size_t size);
short ZoneAllocatorBig_free(void *ptr);
void *ZoneAllocatorBig_realloc(void *ptr, size_t size);

size_t ZoneAllocatorBig_size_get(void *ptr);
void ZoneAllocatorBig_report(void);


#endif // IG_ZONE_ALLOCATOR_BIG_H