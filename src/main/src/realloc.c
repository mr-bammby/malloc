#include "zone_allocator_tiny.h"
#include "zone_allocator_small.h"
#include "zone_allocator_large.h"
#include "malloc.h"
#ifdef FT_BONUS
#include "alloc_manager.h"
#endif /* FT_BONUS */
#include <stddef.h>

void *ft_realloc(void* ptr, size_t size)
{
    short ret;
    short free_done = 0;
    void * temp_ptr = ptr;

    #ifdef FT_BONUS
    pthread_mutex_lock(&alloc_mutex);
    #endif /* FT_BONUS */
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
        ret = ZoneAllocatorLarge_realloc(&temp_ptr, size);
        if (temp_ptr != NULL)
        {
            return temp_ptr;
        }

        temp_ptr = ptr;
        free_done = (ret == 0) ? 1 : 0;
    }
    temp_ptr = ft_malloc(size);
    #ifdef FT_BONUS
    pthread_mutex_unlock(&alloc_mutex);
    #endif /* FT_BONUS */
    return temp_ptr;
}