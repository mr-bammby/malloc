#include "../../ZoneAllocatorTiny/inc_pub/zone_allocator_tiny.h"
#include "../../ZoneAllocatorSmall/inc_pub/zone_allocator_small.h"
#include "../../ZoneAllocatorBig/inc_pub/zone_allocator_big.h"
#include "../inc_pub/malloc.h"
#include <stddef.h>

void *ft_realloc(void* ptr, size_t size)
{
    short ret;
    short free_done = 0;
    void * temp_ptr = ptr;

    ret = ZoneAllocatorTiny_realloc(&temp_ptr, size);
    if (temp_ptr != NULL)
    {
        return temp_ptr;
    }

    temp_ptr = ptr;
    free_done = (ret == 0) ? 1 : 0;

    if (free_done == 0)
    {
        ret = ZoneAllocatorSmall_realloc(&temp_ptr, size);
        if (temp_ptr != NULL)
        {
            return temp_ptr;
        }

        temp_ptr = ptr;
        free_done = (ret == 0) ? 1 : 0;
    }
    if (free_done == 0)
    {
        if (ZoneAllocatorBig_size_get(ptr) != 0)
        {
            ptr = ZoneAllocatorBig_realloc(ptr, size);
            if (ptr != NULL)
            {
                return ptr;
            }
            free_done = 1;
        }
    }
    ptr = ft_malloc(size);
    return ptr;
}