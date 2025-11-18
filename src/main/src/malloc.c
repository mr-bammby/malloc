#include "zone_allocator_tiny.h"
#include "zone_allocator_small.h"
#include "zone_allocator_large.h"

#ifdef FT_BONUS
#include "alloc_manager.h"
#endif /* FT_BONUS */

#include <stddef.h>

/**
 * @brief Internal allocation dispatcher — selects the appropriate zone based on size.
 *
 * @details
 * This function implements the core size-based segregation strategy:
 * - Requests ≤ TINY_ALLOC_SIZE → tiny zone (highly fragmented, high-density)
 * - Requests ≤ SMALL_ALLOC_SIZE_MAX → small zone (balanced fragmentation/performance)
 * - Larger requests → large zone (dedicated mmap per allocation)
 *
 * It performs no locking and must only be called from within a locked context
 * when thread-safety is enabled.
 *
 * @param size Number of bytes requested by the user
 * @return Pointer to the allocated memory block, or NULL if size == 0 or allocation fails
 */
void *no_block_malloc(size_t size)
{
    void *ptr = NULL;

    if (size == 0)
    {
        ptr = NULL;
    }
    else if (size <= TINY_ALLOC_SIZE)
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

    return (ptr);
}

/**
 * @brief Allocates a block of memory of the specified size.
 *
 * The malloc() function allocates `size` bytes of uninitialized memory and
 * returns a pointer to the beginning of the user-accessible portion of the block.
 *
 * @details
 * This implementation is a full, standards-conforming replacement for the
 * system malloc(). It features three segregated allocation zones:
 * - Tiny: for very small allocations (high density, supports splitting/coalescing)
 * - Small: for medium-sized allocations (balanced performance and fragmentation)
 * - Large: for allocations exceeding the small threshold (one mmap per request)
 *
 * Memory returned by malloc() is suitably aligned for any built-in type
 * (at least 8-byte alignment on 64-bit systems, achieved via natural pointer alignment).
 *
 * When thread-safety is enabled (FT_BONUS), the entire operation is protected
 * by a global mutex, ensuring consistency across concurrent threads.
 *
 * @param size Number of bytes to allocate
 * @return On success, a pointer to the allocated memory.
 *         On failure or if size == 0, returns NULL.
 *
 * @note The returned pointer is guaranteed to be properly aligned.
 *       The memory is not initialized (contains indeterminate values).
 */
void *malloc(size_t size)
{
    void *ptr = NULL;

#ifdef FT_BONUS
    /* Critical section: protect entire allocation path for thread safety */
    pthread_mutex_lock(&alloc_mutex);
#endif /* FT_BONUS */

    ptr = no_block_malloc(size);

#ifdef FT_BONUS
    /* End of critical section — allow other threads to proceed */
    pthread_mutex_unlock(&alloc_mutex);
#endif /* FT_BONUS */

    return (ptr);
}
