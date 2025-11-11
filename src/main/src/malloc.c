#include "zone_allocator_tiny.h"
#include "zone_allocator_small.h"
#include "zone_allocator_large.h"
#ifdef FT_BONUS
#include "alloc_manager.h"
#endif /* FT_BONUS */
#include <stddef.h>

void *ft_malloc(size_t size)
{
    
    void *ptr = NULL;
    
    #ifdef FT_BONUS
    pthread_mutex_lock(&alloc_mutex);
    #endif /* FT_BONUS */
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
        ptr = ZoneAllocatorLarge_alloc(size);
    }
    #ifdef FT_BONUS
    pthread_mutex_unlock(&alloc_mutex);
    #endif /* FT_BONUS */

    return (ptr);
}