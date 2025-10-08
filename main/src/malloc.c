#include "../../ZoneAllocatorTiny/inc_pub/zone_allocator_tiny.h"
#include "../../ZoneAllocatorSmall/inc_pub/zone_allocator_small.h"
#include "../../ZoneAllocatorBig/inc_pub/zone_allocator_big.h"
#include <stddef.h>

void *ft_malloc(size_t size)
{
    
    void *ptr = NULL;
    if (size <= TINY_ALLOC_SIZE)
    {
        ptr = ZoneAllocatorTiny_alloc(size);
    }
    
    else if (size <= SMALL_ALLOC_SIZE_MAX)
    {
        ptr = ZoneAllocatorSmall_alloc(size);
    }
    else
    {
        ptr = ZoneAllocatorBig_alloc(size);
    }
    return ptr;
}