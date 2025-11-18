#include "zone_allocator_large.h"
#include "print_utils.h"
#include "alloc_manager.h"
#include <sys/mman.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>

/** Alignment for large allocations — natural pointer alignment (8 bytes on x86_64) */
#define LARGE_ALLOC_ALIGMENT sizeof(void*)

/** Maximum number of concurrent large allocations allowed by the subject */
#define LARGE_ALLOC_NUM 125u

/** Shortcut to the large zone manager structure */
#define LARGE_ALLOC_MANAGER alloc_manager->large_manager

/** Compile-time aligned header sizes using constant-expression alignment */
#define ALIGNED_LARGE_BLOCK_HEADER_SIZE ALIGN_UP_CONST(sizeof(large_block_header_t), LARGE_ALLOC_ALIGMENT)
#define ALIGNED_LARGE_MAP_HEADER_SIZE    ALIGN_UP_CONST(sizeof(large_map_header_t), LARGE_ALLOC_ALIGMENT)

/**
 * @brief Creates and maps a new memory region dedicated to a single large allocation.
 *
 * @details
 * Each large allocation gets its own mmap'ed region containing:
 * - One map header
 * - One block header
 * - User data of at least `size` bytes
 *
 * The total size is rounded up to full pages.
 *
 * @param new_map Pointer to store the address of the new map header
 * @param size    Minimum user-requested data size
 * @return        Usable space after the map header, or 0 on failure
 */
static size_t new_map_add(large_map_header_t **new_map, size_t size)
{
    size_t map_size = 0;
    const int page_size = sysconf(_SC_PAGESIZE); // Get the page size
    const size_t full_size = ALIGN_UP(size, LARGE_ALLOC_ALIGMENT) + ALIGNED_LARGE_MAP_HEADER_SIZE;

    /* Initialize global manager on first large allocation */
    if (AllocManager_init(LARGE_MANAGER) != 0)
    {
        return 0u;
    }

    /* Compute smallest multiple of page_size that fits everything */
    map_size = full_size / page_size;
    map_size = ((full_size % page_size) == 0) ? (map_size) : (map_size + 1);
    map_size *= page_size;

    /* Respect resource limits (RLIMIT_AS / RLIMIT_DATA) */
    if (is_mmap_safe(map_size) != 0)
    {
        AllocManager_uninit(LARGE_MANAGER);
        return 0u;
    }

    *new_map = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (*new_map == MAP_FAILED)
    {
        AllocManager_uninit(LARGE_MANAGER);
        *new_map = NULL;
        return 0u;
    }

    (*new_map)->next = NULL;
    (*new_map)->size = map_size;
    LARGE_ALLOC_MANAGER.large_zone_end = *new_map;

    /* First (and only) block starts right after the aligned map header */
    LARGE_ALLOC_MANAGER.large_zone_end->first_block = (large_block_header_t *)((uint8_t *)LARGE_ALLOC_MANAGER.large_zone_end + ALIGNED_LARGE_MAP_HEADER_SIZE);

    return (map_size - ALIGNED_LARGE_MAP_HEADER_SIZE);
}

/**
 * @brief Initializes the single block inside a freshly mapped large region.
 *
 * @param size          Original user-requested size
 * @param free_map_size Total usable space in the map (after map header)
 * @return              Pointer to the user-accessible memory
 */
static void *new_map_alloc(size_t size, size_t free_map_size)
{
    LARGE_ALLOC_MANAGER.large_zone_end->first_block->used = size;
    LARGE_ALLOC_MANAGER.large_zone_end->first_block->size = free_map_size - ALIGNED_LARGE_BLOCK_HEADER_SIZE;
    LARGE_ALLOC_MANAGER.large_zone_end->first_block->next = NULL;

    LARGE_ALLOC_MANAGER.large_alloc_cnt++;

    return ((void *)((uint8_t *)LARGE_ALLOC_MANAGER.large_zone_end->first_block + ALIGNED_LARGE_BLOCK_HEADER_SIZE));
}

/**
 * @brief Allocates a large memory block (> SMALL_ALLOC_SIZE_MAX).
 *
 * @details
 * Large allocations are handled via dedicated mmap regions.
 * Each call creates a new map containing exactly one block.
 *
 * @param size Requested size in bytes
 * @return Pointer to allocated memory or NULL on failure / size == 0
 */
void *ZoneAllocatorLarge_alloc(size_t size)
{
    size_t free_map_size;
    void *ret = NULL;

    if (size == 0)
    {
        return (ret);
    }

    if (alloc_manager == NULL)
    {
        if (AllocManager_init(LARGE_MANAGER))
        {
            return (ret);
        }
    }

    if (LARGE_ALLOC_MANAGER.large_alloc_cnt >= LARGE_ALLOC_NUM)
    {
        return (ret);
    }

    if (LARGE_ALLOC_MANAGER.large_zone_start == NULL)
    {
        free_map_size = new_map_add(&(LARGE_ALLOC_MANAGER.large_zone_start), size);
    }
    else
    {
        free_map_size = new_map_add(&(LARGE_ALLOC_MANAGER.large_zone_end->next), size);
    }

    if (free_map_size == 0u)
    {
        return (NULL);
    }
    else
    {
        ret = new_map_alloc(size, free_map_size);
    }

    return (ret);
}

/**
 * @brief Retrieves the original requested size of a large allocation.
 *
 * @param ptr User pointer returned by ZoneAllocatorLarge_alloc()
 * @return    Original size passed to alloc(), or 0 if invalid/not found
 */
size_t ZoneAllocatorLarge_size_get(void *ptr)
{
    size_t ret = 0;

    if (ptr == NULL)
    {
        ret = 0; // Invalid pointer
    }
    else if (alloc_manager == NULL)
    {
        ret = 0;
    }
    else if (alloc_manager->large_set == 0)
    {
        ret = 0;
    }
    else
    {
        large_map_header_t *current_map = LARGE_ALLOC_MANAGER.large_zone_start;
        large_block_header_t *current_block;

        while (current_map != NULL)
        {
            current_block = current_map->first_block;
            while (current_block != NULL)
            {
                if ((void *)((uint8_t *)current_block + ALIGNED_LARGE_BLOCK_HEADER_SIZE) == ptr)
                {
                    ret = current_block->used;
                    break;
                }
                current_block = current_block->next;
            }
            if (ret != 0)
            {
                break;
            }
            current_map = current_map->next;
        }
    }
    return (ret);
}

/**
 * @brief Frees a large allocation by unmapping its entire dedicated region.
 *
 * @details
 * Since each large block lives alone in its map, the whole region is returned to the OS.
 *
 * @param ptr Pointer to free
 * @return 0 on success, -1 (invalid), -2 (not found/uninitialized)
 */
short ZoneAllocatorLarge_free(void *ptr)
{
    short ret = 0;

    if (ptr == NULL)
    {
        ret = -1; // Invalid pointer
    }
    else if (alloc_manager == NULL)
    {
        ret = -2;
    }
    else if (alloc_manager->large_set == 0)
    {
        ret = -2;
    }
    else
    {
        large_map_header_t *current_map = LARGE_ALLOC_MANAGER.large_zone_start;
        large_map_header_t *prev_map = NULL;
        large_block_header_t *current_block;

        while (current_map != NULL)
        {
            current_block = current_map->first_block;
            while (current_block != NULL)
            {
                if ((void *)((uint8_t *)current_block + ALIGNED_LARGE_BLOCK_HEADER_SIZE) == ptr)
                {
                    /* Remove map from linked list */
                    if (prev_map == NULL)
                    {
                        LARGE_ALLOC_MANAGER.large_zone_start = current_map->next;
                    }
                    else
                    {
                        prev_map->next = current_map->next;
                    }
                    if (current_map->next == NULL)
                    {
                        LARGE_ALLOC_MANAGER.large_zone_end = prev_map;
                    }

                    LARGE_ALLOC_MANAGER.large_alloc_cnt--;
                    munmap((void *)current_map, current_map->size);
                    AllocManager_uninit(LARGE_MANAGER);
                    break;
                }
                current_block = current_block->next;
            }
            if (current_block != NULL)
            {
                break;
            }
            prev_map = current_map;
            current_map = current_map->next;
        }
        if (current_map == NULL)
        {
            ret = -2; // Not found
        }
    }
    return (ret);
}

/**
 * @brief Resizes a large allocation.
 *
 * @details
 * - Shrink: update used size only
 * - Grow: allocate new block, copy data, free old
 * - Size no longer large: hand off via realloc helper
 *
 * @param ptr  Pointer to user pointer (updated on success)
 * @param size New requested size
 * @return 0 on success, negative on error
 */
short ZoneAllocatorLarge_realloc(void **ptr, size_t size)
{
    short ret = 0;

    if (*ptr == NULL)
    {
        ret = -1; // Invalid pointer
    }
    else if (alloc_manager == NULL)
    {
        *ptr = NULL;
        ret = -2;
    }
    else if (alloc_manager->large_set == 0)
    {
        *ptr = NULL;
        ret = -2;
    }
    else if (size == 0)
    {
        *ptr = NULL;
        ret = -1;
    }
    else
    {
        large_map_header_t *current_map = LARGE_ALLOC_MANAGER.large_zone_start;
        large_map_header_t *prev_map = NULL;
        large_block_header_t *current_block;

        while (current_map != NULL)
        {
            current_block = current_map->first_block;
            while (current_block != NULL)
            {
                if ((void *)((uint8_t *)current_block + ALIGNED_LARGE_BLOCK_HEADER_SIZE) == *ptr)
                {
                    /* Hand off to tiny/small realloc if size falls below large threshold */
                    if ((size < LARGE_ALLOC_SIZE_MIN))
                    {
                        alloc_manager->realloc_hlp.mem_size = current_block->used;
                        alloc_manager->realloc_hlp.mem = *ptr;
                        alloc_manager->realloc_hlp.manager = LARGE_MANAGER;
                        *ptr = NULL;
                        break;
                    }

                    if (current_block->size < size)
                    {
                        void *tmp = *ptr;
                        *ptr = ZoneAllocatorLarge_alloc(size);
                        if (*ptr != NULL)
                        {
                            ft_memcpy(*ptr, tmp, current_block->used);

                            /* Remove old map */
                            if (prev_map == NULL)
                            {
                                LARGE_ALLOC_MANAGER.large_zone_start = current_map->next;
                            }
                            else
                            {
                                prev_map->next = current_map->next;
                            }
                            if (current_map->next == NULL)
                            {
                                LARGE_ALLOC_MANAGER.large_zone_end = prev_map;
                            }
                            LARGE_ALLOC_MANAGER.large_alloc_cnt--;
                            munmap((void *)current_map, current_map->size);
                            AllocManager_uninit(LARGE_MANAGER);
                        }
                        else
                        {
                            ret = -2;
                        }
                    }
                    else
                    {
                        current_block->used = size;  /* In-place shrink */
                    }
                    break;
                }
                current_block = current_block->next;
            }
            if (current_block != NULL)
            {
                break;
            }
            prev_map = current_map;
            current_map = current_map->next;
        }
    }
    return ret;
}

/**
 * @brief Prints all active large allocations in the expected format.
 *
 * Output example:
 *   LARGE : 0x...
 *   0x... - 0x... : size
 */
void ZoneAllocatorLarge_report(void)
{
    if (alloc_manager == NULL)
    {
        return;
    }
    if (LARGE_ALLOC_MANAGER.large_zone_start == NULL)
    {
        return;
    }

    write(1, "LARGE :", 7);  /* 7 characters including trailing space */

    large_map_header_t *current_map = LARGE_ALLOC_MANAGER.large_zone_start;
    large_block_header_t *current_block;

    while (current_map != NULL)
    {
        print_address_as_hex((void *)current_map);  /* Print map base address */
        write(1, "\n", 1);

        current_block = current_map->first_block;
        while (current_block != NULL)
        {
            if (current_block->used != 0u)
            {
                print_address_as_hex((void *)((uint8_t *)current_block + ALIGNED_LARGE_BLOCK_HEADER_SIZE));
                write(1, " - ", 3);
                print_address_as_hex((void *)((uint8_t *)current_block + ALIGNED_LARGE_BLOCK_HEADER_SIZE + current_block->used));
                write(1, " : ", 3);
                print_size(current_block->used);
                write(1, "\n", 1);
            }
            current_block = current_block->next;
        }
        write(1, "\n", 1);
        current_map = current_map->next;
    }
}