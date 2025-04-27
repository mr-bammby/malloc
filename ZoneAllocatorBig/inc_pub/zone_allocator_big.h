#ifndef IG_ZONE_ALLOCATOR_BIG_H
#define IG_ZONE_ALLOCATOR_BIG_H

#include <stdlib.h>

void *ZoneAllocatorBig_alloc(size_t size);
short ZoneAllocatorBig_free(void *ptr);
void *ZoneAllocatorBig_realloc(void *ptr, size_t size);

void *ZoneAllocatorBig_report(void);


#endif // IG_ZONE_ALLOCATOR_BIG_H