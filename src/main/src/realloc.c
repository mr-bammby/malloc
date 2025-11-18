#include "zone_allocator_tiny.h"
#include "zone_allocator_small.h"
#include "zone_allocator_large.h"
#include "malloc_priv.h"
#include "print_utils.h"
#include "alloc_manager.h"
#include <stddef.h>

/**
 * @brief Changes the size of the memory block pointed to by `ptr` to `size` bytes.
 *
 * The realloc() function modifies the size of a previously allocated memory block.
 * The contents of the block are preserved up to the lesser of the old and new sizes.
 *
 * @details
 * This implementation supports full standard behavior:
 * - If `ptr` is NULL → behaves like malloc(size)
 * - If `size` is 0 → behaves like free(ptr), returns NULL
 * - Otherwise → attempts to resize the block in-place when possible
 *
 * The function tries each allocation zone in order (tiny → small → large).
 * Each zone's realloc function may:
 *   • Shrink the block in-place (update used size)
 *   • Expand in-place if adjacent free space exists
 *   • Fail and signal via the global `realloc_hlp` hand-off structure
 *
 * When in-place resize is impossible or the new size belongs to a different zone,
 * the zone signals by setting `realloc_hlp.mem`, `mem_size`, and `manager`.
 * This function then allocates a new block, copies the data, and frees the original.
 *
 * Memory returned is suitably aligned for any object type.
 * When thread-safety is enabled (FT_BONUS), the entire operation is protected
 * by the global mutex to prevent race conditions during pointer searches,
 * block splitting, coalescing, and cross-zone hand-off.
 *
 * @param ptr  Pointer to a memory block previously allocated by malloc(), realloc(),
 *             or aligned_alloc(), or NULL
 * @param size New size for the memory block, in bytes
 *
 * @return
 *   • On success: pointer to the resized (possibly moved) memory block
 *   • On failure: NULL (original block is left untouched)
 *   • If size == 0 and ptr != NULL: NULL (original block is freed)
 *   • If ptr == NULL: equivalent to malloc(size)
 *
 * @note
 *   • The returned pointer may differ from `ptr` even if the size is unchanged.
 *   • The memory beyond the new size is left uninitialized.
 *   • It is undefined behavior to use the original pointer after a successful realloc()
 *     that returned a different address.
 */
void *realloc(void* ptr, size_t size)
{
    short found = 0;
    void * temp_ptr = ptr;

    #ifdef FT_BONUS
    pthread_mutex_lock(&alloc_mutex);
    #endif /* FT_BONUS */
    if (ptr == NULL)
    {
        temp_ptr = no_block_malloc(size);
        found = 2;
    }
    else if (size == 0)
    {
        temp_ptr = NULL;
        found = 2;
    }
    else if(alloc_manager == NULL)
    {
        temp_ptr  = NULL;
        found = 2;
    }
    if (found == 0)
    {
        (void)ZoneAllocatorTiny_realloc(&temp_ptr, size);
        if (temp_ptr != NULL)
        {
            found = 2;
        }
        else
        {
            found = (alloc_manager->realloc_hlp.mem != NULL) ? 1 : 0;
        }
    }
    if (found == 0)
    {
        temp_ptr = ptr;
        (void)ZoneAllocatorSmall_realloc(&temp_ptr, size);
        if (temp_ptr != NULL)
        {
            found = 2;
        }
        else
        {
            found = (alloc_manager->realloc_hlp.mem != NULL) ? 1 : 0;
        }
    }
    if (found == 0)
    {
        temp_ptr = ptr;
        (void)ZoneAllocatorLarge_realloc(&temp_ptr, size);
        if (temp_ptr != NULL)
        {
            found = 2;
        }
        else
        {
            found = (alloc_manager->realloc_hlp.mem != NULL) ? 1 : 0;
        }
    }
    if (found == 1)
    {
        temp_ptr = ptr;
        temp_ptr = no_block_malloc(size);
        if (temp_ptr != NULL)
        {
            ft_memcpy(temp_ptr, alloc_manager->realloc_hlp.mem, alloc_manager->realloc_hlp.mem_size);
            
            switch (alloc_manager->realloc_hlp.manager)
            {
                case TINY_MANAGER:
                    ZoneAllocatorTiny_free(alloc_manager->realloc_hlp.mem);
                    break;
                case SMALL_MANAGER:
                    ZoneAllocatorSmall_free(alloc_manager->realloc_hlp.mem);
                    break;
                case LARGE_MANAGER:
                    ZoneAllocatorTiny_free(alloc_manager->realloc_hlp.mem);
                    break;
                default:
                    break;
            }
            alloc_manager->realloc_hlp.mem = NULL;
        }
    }
    else if (found == 0)
    {
        temp_ptr = NULL;
    }
    
    #ifdef FT_BONUS
    pthread_mutex_unlock(&alloc_mutex);
    #endif /* FT_BONUS */
    return temp_ptr;
}