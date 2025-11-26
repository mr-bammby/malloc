#include "zone_allocator_large.h"
#include "print_utils.h"
#include "alloc_manager.h"
#include <sys/mman.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>

/** Alignment for large allocations — natural pointer alignment (8 bytes on x86_64) */
#define LARGE_ALLOC_ALIGMENT MALLOC_ALIGNMENT

/** Maximum number of concurrent large allocations allowed by the subject */
#define LARGE_ALLOC_NUM 125u

/** Shortcut to the large zone manager structure */
#define LARGE_ALLOC_MANAGER alloc_manager->large_manager

/** Compile-time aligned header sizes using constant-expression alignment */
#define ALIGNED_LARGE_MAP_HEADER_SIZE    ALIGN_UP_CONST(sizeof(large_map_header_t), LARGE_ALLOC_ALIGMENT)


static inline int safe_add_size_t(size_t a, size_t b, size_t *result)
{
    if (a > SIZE_MAX - b) // would overflow
    {          
        return -1;
    }
    *result = a + b;
    return 0;
}

static inline int safe_align_up(size_t value, size_t *result)
{
    size_t aligned = ALIGN_UP(value, LARGE_ALLOC_ALIGMENT);
    if (aligned < value)
    {           // overflow happened
        return -1;
    }
    *result = aligned;

    return 0;
}

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
    size_t aligned_size;
    size_t full_size;
    size_t map_size = 0;
    const int page_size = sysconf(_SC_PAGESIZE);

    if (safe_align_up(size, &aligned_size) != 0)
    {
        return 0u;
    }
    if (safe_add_size_t(aligned_size, ALIGNED_LARGE_MAP_HEADER_SIZE, &full_size) != 0)
    {
        return 0u;
    }

    if (alloc_manager->large_set == 0)
    {
        AllocManager_init(LARGE_MANAGER);
    }

    /* Round up to a multiple of page_size */
    map_size = (full_size + page_size - 1) & ~(page_size - 1);

    if (map_size < full_size)
    {
        return 0u;
    }
    if (is_mmap_safe(map_size) != 0)
    {
        return 0u;
    }

    *new_map = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (*new_map == MAP_FAILED)
    {
        *new_map = NULL;
        return 0u;
    }

    (*new_map)->next = NULL;
    (*new_map)->size = map_size - ALIGNED_LARGE_MAP_HEADER_SIZE;
    (*new_map)->full_size = map_size;
    (*new_map)->used = size;

    return (map_size - ALIGNED_LARGE_MAP_HEADER_SIZE);
}

/**
 * @brief Allocates a large memory block (> SMALL_ALLOC_SIZE_MAX).
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
        if (free_map_size == 0u)
        {
            return (NULL);
        }
        LARGE_ALLOC_MANAGER.large_zone_end = LARGE_ALLOC_MANAGER.large_zone_start;
    }
    else
    {
        free_map_size = new_map_add(&(LARGE_ALLOC_MANAGER.large_zone_end->next), size);
        if (free_map_size == 0u)
        {
            return (NULL);
        }
        LARGE_ALLOC_MANAGER.large_zone_end = LARGE_ALLOC_MANAGER.large_zone_end->next;
    }

    LARGE_ALLOC_MANAGER.large_alloc_cnt++;

    ret = ((void *)((uint8_t *)LARGE_ALLOC_MANAGER.large_zone_end + ALIGNED_LARGE_MAP_HEADER_SIZE));
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

        while (current_map != NULL)
        {
            if ((void *)((uint8_t *)current_map + ALIGNED_LARGE_MAP_HEADER_SIZE) == ptr)
            {
                ret = current_map->used;
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

        while (current_map != NULL)
        {
            if ((void *)((uint8_t *)current_map + ALIGNED_LARGE_MAP_HEADER_SIZE) == ptr)
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
                if (LARGE_ALLOC_MANAGER.large_zone_end == current_map)
                {
                    LARGE_ALLOC_MANAGER.large_zone_end = prev_map;
                }

                LARGE_ALLOC_MANAGER.large_alloc_cnt--;
                munmap((void *)current_map, current_map->full_size);
                if (LARGE_ALLOC_MANAGER.large_alloc_cnt == 0)
                {
                    AllocManager_uninit(LARGE_MANAGER);
                }
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

        while (current_map != NULL)
        {
            if ((void *)((uint8_t *)current_map + ALIGNED_LARGE_MAP_HEADER_SIZE) == *ptr)
            {
                if (current_map->used == 0u)
                {
                    *ptr = NULL;
                    ret = -1;
                    break;
                }
                /* Hand off to tiny/small realloc if size falls below large threshold */
                if ((size < LARGE_ALLOC_SIZE_MIN))
                {
                    alloc_manager->realloc_hlp.mem_size = current_map->used;
                    alloc_manager->realloc_hlp.mem = *ptr;
                    alloc_manager->realloc_hlp.manager = LARGE_MANAGER;
                    *ptr = NULL;
                    break;
                }

                if (current_map->size < size)
                {
                    void *old_user_ptr = *ptr;
                    *ptr = ZoneAllocatorLarge_alloc(size);
                    if (*ptr != NULL)
                    {
                        ft_memcpy(*ptr, old_user_ptr, current_map->used);

                        /* Remove the old map from the linked list */
                        if (prev_map == NULL)
                        {
                            LARGE_ALLOC_MANAGER.large_zone_start = current_map->next;
                        }
                        else
                        {
                            prev_map->next = current_map->next;
                        }

                        /* If the map we just removed was the tail, fix the tail pointer */
                        if (LARGE_ALLOC_MANAGER.large_zone_end == current_map)
                        {
                            LARGE_ALLOC_MANAGER.large_zone_end = prev_map;
                        }

                        LARGE_ALLOC_MANAGER.large_alloc_cnt--;
                        munmap((void *)current_map, current_map->full_size);

                        if (LARGE_ALLOC_MANAGER.large_alloc_cnt == 0)
                        {
                            AllocManager_uninit(LARGE_MANAGER);
                        }
                    }
                    else
                    {
                        ret = -2;
                    }
                }
                else
                {
                    current_map->used = size; /* In-place shrink */
                }
                break;
            }
            size_t temp;
            if (safe_add_size_t((size_t)current_map, current_map->full_size, &temp) == 0)
            {
                if (((void *)current_map <= *ptr) && ((void *)(temp) > *ptr))
                {
                    *ptr = NULL;
                    ret = -1;
                    break;
                }
            }
            prev_map = current_map;
            current_map = current_map->next;
        }
        if (current_map == NULL)
        {
            *ptr = NULL;
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

    while (current_map != NULL)
    {
        print_address_as_hex((void *)current_map);  /* Print map base address */
        write(1, "\n", 1);

        if (current_map->used != 0u)
        {
            print_address_as_hex((void *)((uint8_t *)current_map + ALIGNED_LARGE_MAP_HEADER_SIZE));
            write(1, " - ", 3);
            print_address_as_hex((void *)((uint8_t *)current_map + ALIGNED_LARGE_MAP_HEADER_SIZE + current_map->used));
            write(1, " : ", 3);
            print_size(current_map->used);
            write(1, "\n", 1);
        }
        write(1, "\n", 1);
        current_map = current_map->next;
    }
}

void ZoneAllocatorLarge_dump(void)
{
    if (alloc_manager == NULL)
    {
        return;
    }
    if (LARGE_ALLOC_MANAGER.large_zone_start == NULL)
    {
        return;
    }

    write(1, "LARGE dump :", 12);  /* 7 characters including trailing space */

    large_map_header_t *current_map = LARGE_ALLOC_MANAGER.large_zone_start;

    while (current_map != NULL)
    {
        print_address_as_hex((void *)current_map);  /* Print map base address */
        write(1, "\n", 1);

        if (current_map->used != 0u)
        {
            write(1, "Alloc: ", 7);
            print_address_as_hex((void *)((uint8_t *)current_map + ALIGNED_LARGE_MAP_HEADER_SIZE));
            write(1, " - ", 3);
            print_address_as_hex((void *)((uint8_t *)current_map + ALIGNED_LARGE_MAP_HEADER_SIZE + current_map->used));
            write(1, " : ", 3);
            print_size(current_map->used);
            write(1, "\n", 1);

            print_dump_header(LARGE_ALLOC_ALIGMENT);
            write(1, "\n", 1);
            size_t size = current_map->used;
            size_t offset = 0u;
            uint8_t *start = ((uint8_t *)current_map + ALIGNED_LARGE_MAP_HEADER_SIZE);
            while (size > offset)
            {
                print_address_as_hex(start + offset);
                write(1, ": ", 2);
                size_t print_size = ((size - offset) < LARGE_ALLOC_ALIGMENT) ? (size - offset) : LARGE_ALLOC_ALIGMENT;
                print_dump((void *)(start + offset), print_size);
                write(1, "\n", 1);
                offset += LARGE_ALLOC_ALIGMENT;
            }
        }
        write(1, "\n", 1);
        current_map = current_map->next;
    }
}