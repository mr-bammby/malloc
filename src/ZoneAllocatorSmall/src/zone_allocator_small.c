#include "zone_allocator_small.h"
#include "print_utils.h"
#include "alloc_manager.h"
#include <sys/mman.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>

//#define SMALL_BLOCK_HEADER_SIZE sizeof(small_block_header_t) /* Size of the header */
//#define SMALL_MAP_HEADER_SIZE sizeof(small_map_header_t) /* Size of the header */
#define SMALL_MAP_DEFAULT_ALLOC 16ul /* in pages */
#define SMALL_ALLOC_ALIGMENT sizeof(void*) /* Alignment of the small allocation */
#define SMALL_ALLOC_NUM 125u
#define SMALL_ALLOC_MANAGER alloc_manager->small_manager

#define ALIGNED_SMALL_BLOCK_HEADER_SIZE ALIGN_UP_CONST(sizeof(small_block_header_t), SMALL_ALLOC_ALIGMENT)
#define ALIGNED_SMALL_MAP_HEADER_SIZE ALIGN_UP_CONST(sizeof(small_map_header_t), SMALL_ALLOC_ALIGMENT)

/**
 * @brief Adds a new memory map for small allocations.
 *
 * @details
 * Allocates a new memory region using mmap() with a size that is a multiple of
 * the page size, starting from SMALL_MAP_DEFAULT_ALLOC pages and halving until
 * mmap-safe. Initializes the map header, sets up the first block pointer after
 * alignment padding, and links it into the chain (either as the first map or
 * appended to the current end). Returns the usable size after header alignment.
 *
 * @param new_map Pointer to store the new map header.
 * @param size Minimum size needed (used to ensure map is large enough).
 * @return size_t Usable space in the new map (after header), or 0 on failure.
 */
static size_t new_map_add(small_map_header_t **new_map, size_t size)
{
    const size_t page_size = sysconf(_SC_PAGESIZE);
    size_t map_size = page_size * SMALL_MAP_DEFAULT_ALLOC; /* Get the page size */
    if (AllocManager_init(SMALL_MANAGER) != 0)
    {
        return 0u;
    }
    while (is_mmap_safe(map_size) != 0)
    {
        if (map_size < size)
        {
            AllocManager_uninit(SMALL_MANAGER);
            return 0u;
        }
        map_size /= 2u;
        if (map_size < page_size)
        {
            AllocManager_uninit(SMALL_MANAGER);
            return 0u;
        }
    }
    *new_map = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (*new_map == MAP_FAILED)
    {
        *new_map = NULL;
        AllocManager_uninit(SMALL_MANAGER);
        return 0u;
    }
    (*new_map)->next = NULL;
    (*new_map)->cnt = 1;
    (*new_map)->size = map_size;
    SMALL_ALLOC_MANAGER.small_zone_end = *new_map;
    SMALL_ALLOC_MANAGER.small_zone_end->first_block = (small_block_header_t *)((uint8_t *)SMALL_ALLOC_MANAGER.small_zone_end + ALIGNED_SMALL_MAP_HEADER_SIZE);
    return (map_size - ALIGNED_SMALL_MAP_HEADER_SIZE);
}

/**
 * @brief Allocates the first block in a newly created map.
 *
 * @details
 * Sets up the first block in the new map with the requested size. Aligns the
 * full block size (data + header) to SMALL_ALLOC_ALIGMENT. If enough space
 * remains after allocation, splits the remainder into a free block. Updates
 * allocation counters and returns the user pointer.
 *
 * @param size Requested data size.
 * @param free_map_size Total usable space in the new map.
 * @return void* Pointer to allocated memory, or NULL if not enough space.
 */
static void *new_map_alloc(size_t size, size_t free_map_size)
{
    SMALL_ALLOC_MANAGER.small_zone_end->first_block->used = size;
    SMALL_ALLOC_MANAGER.small_zone_end->first_block->size = ALIGN_UP(size, SMALL_ALLOC_ALIGMENT);
    if ((free_map_size - SMALL_ALLOC_MANAGER.small_zone_end->first_block->size) > (2 * ALIGNED_SMALL_BLOCK_HEADER_SIZE)) // Smallest possible allocation
    {
        SMALL_ALLOC_MANAGER.small_zone_end->first_block->next = (small_block_header_t *)((uint8_t *)SMALL_ALLOC_MANAGER.small_zone_end->first_block + SMALL_ALLOC_MANAGER.small_zone_end->first_block->size + ALIGNED_SMALL_BLOCK_HEADER_SIZE);
        SMALL_ALLOC_MANAGER.small_zone_end->first_block->next->next = NULL;
        free_map_size -= (SMALL_ALLOC_MANAGER.small_zone_end->first_block->size + ALIGNED_SMALL_BLOCK_HEADER_SIZE);
        SMALL_ALLOC_MANAGER.small_zone_end->first_block->next->size = free_map_size - ALIGNED_SMALL_BLOCK_HEADER_SIZE;
        SMALL_ALLOC_MANAGER.small_zone_end->first_block->next->used = 0;
    }
    else
    {
        SMALL_ALLOC_MANAGER.small_zone_end->first_block->next = NULL;
    }
    SMALL_ALLOC_MANAGER.small_alloc_cnt++;
    return ((uint8_t *)SMALL_ALLOC_MANAGER.small_zone_end->first_block + ALIGNED_SMALL_BLOCK_HEADER_SIZE);
}

/**
 * @brief Allocates from an existing map by finding a suitable free block.
 *
 * @details
 * Traverses all maps and their block chains to find a free block large enough
 * for the request. Aligns the required size, then splits the free block if
 * possible. Updates counters and returns the user pointer. Returns NULL if
 * no suitable block is found.
 *
 * @param size Requested data size.
 * @return void* Pointer to allocated memory, or NULL if not found.
 */
static void *old_map_alloc(size_t size)
{
    small_map_header_t *current_map = SMALL_ALLOC_MANAGER.small_zone_start;
    small_block_header_t *current_block, *new_block;
    size_t aligned_size, diff;
    void *ret = NULL;
    aligned_size = ALIGN_UP(size, SMALL_ALLOC_ALIGMENT); 
    while (current_map != NULL)
    {
        current_block = current_map->first_block;
        while (current_block != NULL)
        {
            if ((current_block->used == 0) && (current_block->size >= aligned_size))
            {
                current_block->used = size;
                diff = current_block->size - aligned_size;
                if (diff > (ALIGNED_SMALL_BLOCK_HEADER_SIZE + SMALL_ALLOC_SIZE_MIN))
                {
                    new_block = (void *)((uint8_t *)current_block + aligned_size + ALIGNED_SMALL_BLOCK_HEADER_SIZE);
                    new_block->next = current_block->next;
                    current_block->next = new_block;
                    new_block->size = diff - ALIGNED_SMALL_BLOCK_HEADER_SIZE;
                    new_block->used = 0;
                    current_block->size = aligned_size;
;
                }
                ret = (void *)((uint8_t *)current_block + ALIGNED_SMALL_BLOCK_HEADER_SIZE);
                current_map->cnt++;
                SMALL_ALLOC_MANAGER.small_alloc_cnt++;
                break;
            }
            current_block = current_block->next;
        }
        if (ret != NULL)
        {
            break;
        }
        current_map = current_map->next;
    }
    return (ret);
}

/**
 * @brief Allocates a block of memory of the given size in the small zone.
 *
 * @details
 * First attempts to allocate from existing maps using old_map_alloc(). If that
 * fails and the global allocation limit (SMALL_ALLOC_NUM) is not reached, creates
 * a new map via new_map_add() and allocates the first block in it. Returns NULL
 * on failure or invalid size.
 *
 * @param size Size of the memory block to allocate.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
void *ZoneAllocatorSmall_alloc(size_t size)
{
    size_t free_map_size;
    void *ret = NULL;
    if (size == 0)
    {
        return (ret);
    }
    if (alloc_manager == NULL)
    {
        if (AllocManager_init(SMALL_MANAGER))
        {
            return (ret);
        }
    }
    if (SMALL_ALLOC_MANAGER.small_alloc_cnt >= SMALL_ALLOC_NUM)
    {
        return (ret);
    }
    ret = old_map_alloc(size);
    if (ret == NULL)
    {
        if (SMALL_ALLOC_MANAGER.small_zone_start == NULL)
        {
            free_map_size = new_map_add(&(SMALL_ALLOC_MANAGER.small_zone_start), size);
        }
        else
        {
            free_map_size = new_map_add(&(SMALL_ALLOC_MANAGER.small_zone_end->next), size);
        }
        if (free_map_size == 0u)
        {
            ret = NULL;
        }
        else
        {
            ret = new_map_alloc(size, free_map_size);
        }
    }
    return (ret);
}

/**
 * @brief Retrieves the size of the memory block pointed to by ptr.
 *
 * @details
 * Searches all maps and blocks to find the one corresponding to the given user
 * pointer. Returns the stored used size from the block header. Returns 0 if
 * the pointer is invalid, not found, or the manager is uninitialized.
 *
 * @param ptr Pointer to a previously allocated small block.
 * @return size_t The size of the block, or 0 if invalid.
 */
size_t ZoneAllocatorSmall_size_get(void *ptr)
{
    size_t ret = 0;
    if (ptr == NULL)
    {
        ret = 0; /* Invalid pointer */
    }
    else if (alloc_manager == NULL)
    {
        ret = 0;
    }
    else if (alloc_manager->small_set == 0)
    {
        ret = 0;
    }
    else
    {
        small_map_header_t *current_map = SMALL_ALLOC_MANAGER.small_zone_start;
        small_block_header_t *current_block;
        while (current_map != NULL)
        {
            current_block = current_map->first_block;
            while (current_block != NULL)
            {
                if ((void *)((uint8_t *)current_block + ALIGNED_SMALL_BLOCK_HEADER_SIZE) == ptr)
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
 * @brief Defragments adjacent free blocks.
 *
 * @details
 * Merges the current free block with the next free block if adjacent. If a
 * previous free block is provided, merges backward as well. Updates size and
 * next pointers accordingly. Used during free() to reduce fragmentation.
 *
 * @param prev_block Previous block (may be NULL).
 * @param block Current block to defragment (must not be NULL).
 */
static void defrag(small_block_header_t* prev_block, small_block_header_t *block)
{
    if (block != NULL)
    {
        small_block_header_t* next_block = block->next;
        size_t size = block->size;
        small_block_header_t* temp_block = block->next;
        if (next_block != NULL)
        {
            if (next_block->used == 0)
            {
                size += next_block->size;
                size += ALIGNED_SMALL_BLOCK_HEADER_SIZE;
                temp_block = next_block->next;
            }
        }
        if (prev_block != NULL)
        {
            if (prev_block->used == 0u)
            {
                size += prev_block->size;
                size += ALIGNED_SMALL_BLOCK_HEADER_SIZE;
                prev_block->size = size;
                prev_block->next = temp_block;
                return;
            }
        }
        block->size = size;
        block->next = temp_block;
        return;
    }
}

/**
 * @brief Frees the memory block pointed to by ptr.
 *
 * @details
 * Locates the block, marks it as free, decrements counters. If the map becomes
 * empty, unmaps it and updates the chain. Otherwise, calls defrag() to merge
 * with adjacent free blocks. Returns error codes on invalid input.
 *
 * @param ptr Pointer to the block to free.
 * @return short 0 on success, -1 for invalid pointer, -2 for not found or uninitialized.
 */
short ZoneAllocatorSmall_free(void *ptr)
{
    short ret = 0;
    if (ptr == NULL)
    {
        ret = -1; /* Invalid pointer */
    }
    else if (alloc_manager == NULL)
    {
        ret = -2;
    }
    else if (alloc_manager->small_set == 0)
    {
        ret = -2;
    }
    else
    {
        small_map_header_t *current_map = SMALL_ALLOC_MANAGER.small_zone_start;
        small_map_header_t *prev_map = NULL;
        small_block_header_t *current_block, *prev_block;
        while (current_map != NULL)
        {
            prev_block = NULL;
            current_block = current_map->first_block;
            while (current_block != NULL)
            {
                if ((void *)((uint8_t *)current_block + ALIGNED_SMALL_BLOCK_HEADER_SIZE) == ptr)
                {
                    current_block->used = 0; /* Freed */
                    current_map->cnt--;
                    SMALL_ALLOC_MANAGER.small_alloc_cnt--;
                    if (current_map->cnt == 0)
                    {
                        if (prev_map == NULL)
                        {
                            SMALL_ALLOC_MANAGER.small_zone_start = current_map->next;
                        }
                        else
                        {
                            prev_map->next = current_map->next;
                        }
                        if (current_map->next == NULL)
                        {
                            SMALL_ALLOC_MANAGER.small_zone_end = prev_map;
                        }
                        munmap((void *)current_map, current_map->size);
                        AllocManager_uninit(SMALL_MANAGER);
                    }
                    else
                    {
                        defrag(prev_block, current_block);
                    }
                    break;
                }
                prev_block = current_block;
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
            ret = -2; /* Not found */
        }
    }
    return (ret);
}

/**
 * @brief Helper for realloc: allocates new block, copies data, frees old.
 *
 * @details
 * Used when in-place expansion is not possible. Allocates a new block of the
 * requested size, copies the old data, then frees the old block (with defrag
 * and possible unmap). Returns error if allocation fails.
 *
 * @param ptr Pointer to update.
 * @param size New size.
 * @param current_map Map containing old block.
 * @param prev_map Previous map.
 * @param current_block Old block.
 * @param prev_block Previous block.
 * @return short 0 on success, -2 on allocation failure.
 */
static short realloc_cpy_free_hlp(void **ptr, size_t size, small_map_header_t *current_map, small_map_header_t *prev_map, \
									small_block_header_t *current_block, small_block_header_t *prev_block)
{
    short ret = 0;
    void *tmp = *ptr;
    *ptr = ZoneAllocatorSmall_alloc(size);
    if (*ptr != NULL)
    {
        ft_memcpy(*ptr, tmp, current_block->used);
        current_block->used = 0; /* Freed */
        current_map->cnt--;
        SMALL_ALLOC_MANAGER.small_alloc_cnt--;
        if (current_map->cnt == 0)
        {
            if (prev_map == NULL)
            {
                SMALL_ALLOC_MANAGER.small_zone_start = current_map->next;
            }
            else
            {
                prev_map->next = current_map->next;
            }
            if (current_map->next == NULL)
            {
                SMALL_ALLOC_MANAGER.small_zone_end = prev_map;
            }
            munmap((void *)current_map, current_map->size);
            AllocManager_uninit(SMALL_MANAGER);
        }
        else
        {
            defrag(prev_block, current_block);
        }
    }
    else
    {
        ret = -2;
    }
    return ret;
}

/**
 * @brief Resizes a memory block in the small zone.
 *
 * @details
 * Attempts in-place expansion by merging with the next block if possible and
 * sufficient. If shrinking, updates used size. If expansion not possible or
 * size out of small range, uses realloc_cpy_free_hlp() or hands off via
 * realloc_hlp. Updates *ptr to NULL on handover or error.
 *
 * @param ptr Pointer to the pointer to reallocate.
 * @param size New requested size.
 * @return short 0 on success, negative on error.
 */
short ZoneAllocatorSmall_realloc(void **ptr, size_t size)
{
    short ret = 0;
    if (ptr == NULL)
    {
        *ptr = NULL;
        ret = -1; /* Invalid pointer */
    }
    else if (alloc_manager == NULL)
    {
        *ptr = NULL;
        ret = -2;
    }
    else if (alloc_manager->small_set == 0)
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
        small_map_header_t *current_map = SMALL_ALLOC_MANAGER.small_zone_start;
        small_map_header_t *prev_map = NULL;
        small_block_header_t *current_block, *prev_block;
        while (current_map != NULL)
        {
            prev_block = NULL;
            current_block = current_map->first_block;
            while (current_block != NULL)
            {
                if ((void *)((uint8_t *)current_block + ALIGNED_SMALL_BLOCK_HEADER_SIZE) == *ptr)
                {
                    if ((size < SMALL_ALLOC_SIZE_MIN) || (size > SMALL_ALLOC_SIZE_MAX))
                    {
                        alloc_manager->realloc_hlp.mem_size = current_block->used;
                        alloc_manager->realloc_hlp.mem = *ptr;
                        alloc_manager->realloc_hlp.manager = SMALL_MANAGER;
                        *ptr = NULL;
                        break;
                    }
                    small_block_header_t* next_block = current_block->next;
                    size_t aligned_size = ALIGN_UP(size, SMALL_ALLOC_ALIGMENT);
                    if (current_block->size < size)
                    {
                        if ((next_block != NULL) && (next_block->used == 0u))
                        {
                            size_t max_size = current_block->size + next_block->size + ALIGNED_SMALL_BLOCK_HEADER_SIZE;
                            if (max_size >= aligned_size)
                            {
                                current_block->used = size;
                                size_t size_diff = max_size - aligned_size;
                                if (size_diff > (ALIGNED_SMALL_BLOCK_HEADER_SIZE + SMALL_ALLOC_SIZE_MIN))
                                {
                                    // Split: create new free block
                                    current_block->next = (void *)((uint8_t *)current_block + aligned_size + ALIGNED_SMALL_BLOCK_HEADER_SIZE);
                                    current_block->next->next = next_block->next;
                                    current_block->next->used = 0;
                                    current_block->next->size = size_diff - ALIGNED_SMALL_BLOCK_HEADER_SIZE;
                                    current_block->size = aligned_size;
                                }
                                else
                                {
                                    // Absorb whole without split
                                    current_block->next = next_block->next;
                                    current_block->size = max_size;
                                }
                            }
                            else
                            {
                                ret = realloc_cpy_free_hlp(ptr, size, current_map, prev_map, current_block, prev_block);
                            }
                        }
                        else
                        {
                            ret = realloc_cpy_free_hlp(ptr, size, current_map, prev_map, current_block, prev_block);
                        }
                    }
                    else
                    {
                        current_block->used = size;
                    }
                    break;
                }
                prev_block = current_block;
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
 * @brief Prints a report of all allocated blocks in the small zone.
 *
 * @details
 * For each map, prints its base address. Then for each used block in the map,
 * prints the user start address, end address (start + used size), and size.
 * Only executes if at least one map exists.
 */
void ZoneAllocatorSmall_report(void)
{
    if (alloc_manager == NULL)
    {
        return;
    }
    if (SMALL_ALLOC_MANAGER.small_zone_start == NULL)
    {
        return;
    }
    write(1, "SMALL : ", 7);
    small_map_header_t *current_map = SMALL_ALLOC_MANAGER.small_zone_start;
    small_block_header_t *current_block;
    while (current_map != NULL)
    {
        print_address_as_hex((void *)current_map); /* Print the start address of map */
        write(1, "\n", 1);
        current_block = current_map->first_block; /* Set the current block */
        while (current_block != NULL)
        {
            if (current_block->used != 0u)
            {
                print_address_as_hex((void *)((uint8_t *)current_block + ALIGNED_SMALL_BLOCK_HEADER_SIZE)); /* Print the address of the block */
                write(1, " - ", 3);
                print_address_as_hex((void *)((uint8_t *)current_block + ALIGNED_SMALL_BLOCK_HEADER_SIZE + current_block->used)); /* Print the end address */
                write(1, " : ", 3);
                print_size(current_block->used); /* Print the size of the block */
                write(1, "\n", 1);
            }
            current_block = current_block->next; /* Move to the next block */
        }
        write(1, "\n", 1);
        current_map = current_map->next;
    }
}
