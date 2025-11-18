#include "zone_allocator_tiny.h"
#include "zone_allocator_small.h"
#include "zone_allocator_large.h"
#ifdef FT_BONUS
#include "alloc_manager.h"
#endif /* FT_BONUS */
#include <stddef.h>

/**
 * @brief Deallocates a memory block previously allocated by malloc().
 *
 * The free() function releases the memory block pointed to by `ptr`, making it
 * available for future allocations. The pointer must have been returned by a
 * previous call to malloc(), realloc(), or a related function within this
 * allocator implementation.
 *
 * @details
 * This implementation uses segregated allocation zones (tiny, small, large)
 * and attempts to free the block in each zone in order of increasing size:
 * 1. Tiny zone  (most likely for small pointers)
 * 2. Small zone (next most likely)
 * 3. Large zone (dedicated mmap regions)
 *
 * The first zone that recognizes the pointer as one of its own performs the
 * deallocation. Tiny and small zones may coalesce adjacent free blocks to
 * reduce fragmentation. Large zone blocks result in an immediate `munmap()` of
 * their entire memory region.
 *
 * Passing NULL is a no-op and is explicitly allowed.
 * Passing an invalid or already-freed pointer results in undefined behavior.
 *
 * When thread-safety is enabled (FT_BONUS), the entire operation is serialized
 * using the global allocation mutex to prevent race conditions during linked-list
 * manipulation and memory unmapping.
 *
 * @param ptr Pointer to the memory block to be freed, or NULL
 *
 * @note
 * - This function has no return value (void) as per the C standard.
 * - Memory is not zeroed or modified in any way upon release.
 * - The pointer becomes invalid immediately after the call.
 */
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