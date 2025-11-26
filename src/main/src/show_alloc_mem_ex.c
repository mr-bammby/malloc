#include "zone_allocator_tiny.h"
#include "zone_allocator_small.h"
#include "zone_allocator_large.h"

#ifdef FT_BONUS
#include "alloc_manager.h"
#endif /* FT_BONUS */

/**
 * @brief Displays all currently active memory allocations across all zones.
 *
 * @details
 * Prints a complete memory map of allocated blocks in the following order:
 * 1. Tiny allocations
 * 2. Small allocations
 * 3. Large allocations
 *
 * Each zone's report function is responsible for formatting its blocks according
 * to the expected output specification (hex addresses, size in decimal + hex).
 *
 * When thread-safety is enabled (FT_BONUS), the entire operation is protected
 * by the global allocation mutex to ensure a consistent snapshot even under
 * concurrent access.
 */
void show_alloc_mem_ex(void)
{
#ifdef FT_BONUS
    /* Serialize access to allocation state — prevents torn reads during reporting */
    if (pthread_mutex_lock(&alloc_mutex))
    {
        write(STDOUT_FILENO, "Mutex fail\n", 11);
        return;
    }
#endif /* FT_BONUS */

    /* Report allocations from each zone in ascending size order */
    ZoneAllocatorTiny_dump();
    ZoneAllocatorSmall_dump();
    ZoneAllocatorLarge_dump();

#ifdef FT_BONUS
    /* Release the global lock — other threads may now proceed */
    pthread_mutex_unlock(&alloc_mutex);
#endif /* FT_BONUS */

    return;
}