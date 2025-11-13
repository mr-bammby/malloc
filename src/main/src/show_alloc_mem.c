#include "zone_allocator_tiny.h"
#include "zone_allocator_small.h"
#include "zone_allocator_large.h"
#ifdef FT_BONUS
#include "alloc_manager.h"
#endif /* FT_BONUS */

void show_alloc_mem(void)
{
    #ifdef FT_BONUS
    pthread_mutex_lock(&alloc_mutex);
    #endif /* FT_BONUS */
    ZoneAllocatorTiny_report();
    ZoneAllocatorSmall_report();
    ZoneAllocatorLarge_report();
    #ifdef FT_BONUS
    pthread_mutex_unlock(&alloc_mutex);
    #endif /* FT_BONUS */
    return;
}