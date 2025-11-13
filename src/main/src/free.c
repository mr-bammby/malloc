#include "zone_allocator_tiny.h"
#include "zone_allocator_small.h"
#include "zone_allocator_large.h"
#ifdef FT_BONUS
#include "alloc_manager.h"
#endif /* FT_BONUS */
#include <stddef.h>

void free(void* ptr)
{
    short ret;

    #ifdef FT_BONUS
    pthread_mutex_lock(&alloc_mutex);
    #endif /* FT_BONUS */
    ret = ZoneAllocatorTiny_free(ptr);
    if (ret != 0)
    {
        ret = ZoneAllocatorSmall_free(ptr);
    }
    if (ret != 0)
    {
        ZoneAllocatorLarge_free(ptr);
    }
    #ifdef FT_BONUS
    pthread_mutex_unlock(&alloc_mutex);
    #endif /* FT_BONUS */
    return;
}