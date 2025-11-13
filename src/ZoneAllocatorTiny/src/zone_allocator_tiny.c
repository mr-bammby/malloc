#include "zone_allocator_tiny.h"
#include "print_utils.h"
#include "alloc_manager.h"
#include <sys/mman.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>

#define TINY_ALLOC_COUNT 125u /* Number of allocations */
#define TINY_ZONE_SIZE (TINY_ALLOC_SIZE * TINY_ALLOC_COUNT) + TINY_ALLOC_COUNT /* Total size of the tiny zone */
#define TINY_ALLOC_ALIGMENT 8u /* Alignment of the tiny allocation */

#define TINY_ALLOC_MANAGER alloc_manager->tiny_manager

/**
 * @brief Allocates a block of memory of the given size.
 *
 * @details
 * This function attempts to allocate a block of the requested size within the
 * pre-mapped tiny zone. If the zone is not yet initialized, it will be created
 * using mmap(). The allocation metadata is stored in the first part of the
 * mapped region, followed by the actual allocatable memory. Each slot in the
 * zone is of fixed size TINY_ALLOC_SIZE, and the metadata byte at the beginning
 * of the zone stores the actual requested size for later retrieval.
 *
 * @param[ingit] size Size of the memory block to allocate (must be <= TINY_ALLOC_SIZE).
 * @return void* Pointer to the allocated memory, or NULL on failure or invalid size.
 */
void *ZoneAllocatorTiny_alloc(size_t size)
{
    if ((size == 0) || (size > TINY_ALLOC_SIZE))
    {
        return (NULL);
    }
    if (alloc_manager == NULL)
    {
        if (AllocManager_init(TINY_MANAGER))
        {
            return NULL;
        }
    }
    if (TINY_ALLOC_MANAGER.tiny_zone_map == NULL)
    {
        if (AllocManager_init(TINY_MANAGER))
        {
            return NULL;
        }
        const size_t page_size = sysconf(_SC_PAGESIZE); /* Get the page size */
        TINY_ALLOC_MANAGER.tiny_zone_mapped_size = TINY_ZONE_SIZE / page_size; /* Calculate the number of aligned blocks */
        TINY_ALLOC_MANAGER.tiny_zone_mapped_size = (TINY_ALLOC_MANAGER.tiny_zone_mapped_size * page_size) + ((TINY_ZONE_SIZE % page_size == 0u) ? (0u) : (page_size)); /* Align to page size */
        if (is_mmap_safe(TINY_ALLOC_MANAGER.tiny_zone_mapped_size) != 0)
        {
            TINY_ALLOC_MANAGER.tiny_zone_mapped_size = 0;
            AllocManager_uninit(TINY_MANAGER);
            return NULL;
        }
        TINY_ALLOC_MANAGER.tiny_zone_map = mmap(NULL, TINY_ALLOC_MANAGER.tiny_zone_mapped_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (TINY_ALLOC_MANAGER.tiny_zone_map == MAP_FAILED)
        {
            TINY_ALLOC_MANAGER.tiny_zone_map = NULL;
            AllocManager_uninit(TINY_MANAGER);
            return NULL; /* Allocation failed */
        }
        uint8_t aligned_count = TINY_ALLOC_COUNT / TINY_ALLOC_ALIGMENT; /* Calculate the number of aligned blocks */
        aligned_count = (aligned_count * TINY_ALLOC_ALIGMENT) + ((TINY_ALLOC_COUNT % TINY_ALLOC_ALIGMENT == 0u) ? (0u) : (TINY_ALLOC_ALIGMENT)); /* Align to 16 */
        TINY_ALLOC_MANAGER.tiny_zone_start = TINY_ALLOC_MANAGER.tiny_zone_map + aligned_count; /* Set the start pointer */
        TINY_ALLOC_MANAGER.tiny_zone_end = TINY_ALLOC_MANAGER.tiny_zone_start + TINY_ALLOC_SIZE * TINY_ALLOC_COUNT; /* Set the end pointer */
        TINY_ALLOC_MANAGER.tiny_alloc_cnt = 0u;
    }

    for (uint8_t i = 0u; i < TINY_ALLOC_COUNT; i++)
    {
        if ((((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[i]) == 0u)
        {
            ((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[i] = size; /* Mark the block as used */
            TINY_ALLOC_MANAGER.tiny_alloc_cnt++;
            return ((void *)((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_start + ((size_t)i * TINY_ALLOC_SIZE))); /* Return the pointer to the allocated memory */
        }
    }

    return (NULL);
}

/**
 * @brief Retrieves the size of the memory block pointed to by ptr.
 *
 * @details
 * Validates that the pointer belongs to the tiny zone and is properly aligned
 * to a TINY_ALLOC_SIZE boundary. The function computes the index from the
 * offset within the data region and reads the stored size from the metadata
 * map. Returns 0 if the pointer is invalid, out of range, or misaligned.
 *
 * @param ptr Pointer to a previously allocated tiny block.
 * @return size_t The size of the block, or 0 if invalid.
 */
size_t ZoneAllocatorTiny_size_get(void *ptr)
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
    else if (alloc_manager->tiny_set == 0)
    {
        ret = 0;
    }
    else if (ptr < TINY_ALLOC_MANAGER.tiny_zone_start || ptr > TINY_ALLOC_MANAGER.tiny_zone_end)
    {
        ret = 0; /* Pointer out of range */
    }
    else if (ptr == TINY_ALLOC_MANAGER.tiny_zone_map)
    {
        ret = 0; /* Pointer is the map of the zone */
    }
    else
    {
        uint8_t index = ((uint8_t*)ptr - (uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_start) / TINY_ALLOC_SIZE; /* Calculate the index of the block */
        if (index * TINY_ALLOC_SIZE + (uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_start != (uint8_t *)ptr)
        {
            ret = 0; /* Pointer not aligned */
        }
        else
        {
            ret = ((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[index]; /* Return the size of the block */
        }
    }
    return (ret);
}

/**
 * @brief Frees the memory block pointed to by ptr.
 *
 * @details
 * Marks the block as free in the metadata map by setting its size to 0.
 * Decrements the allocation counter. If this was the last allocation in the
 * zone (tiny_alloc_cnt reaches 0), the entire mapped region is unmapped using
 * munmap() and the tiny manager is uninitialized.
 *
 * @param ptr Pointer to the block to free.
 * @return short 0 on success, -1 for invalid pointer, -2 for uninitialized manager or out-of-range.
 */
short ZoneAllocatorTiny_free(void *ptr)
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
    else if (alloc_manager->tiny_set == 0)
    {
        ret = -2;
    }
    else if (ptr < TINY_ALLOC_MANAGER.tiny_zone_start || ptr > TINY_ALLOC_MANAGER.tiny_zone_end)
    {
        ret = -2; /* Pointer out of range */
    }
    else
    {
        uint8_t index = ((uint8_t*)ptr - (uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_start) / TINY_ALLOC_SIZE; /* Calculate the index of the block */
        if (index * TINY_ALLOC_SIZE + (uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_start != (uint8_t *)ptr)
        {
            ret = -2; /* Pointer not aligned */
        }
        else /* Free the block */
        {
            ((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[index] = 0u; /* Mark the block as free */
            TINY_ALLOC_MANAGER.tiny_alloc_cnt--;
            if (TINY_ALLOC_MANAGER.tiny_alloc_cnt == 0u)
            {
                munmap(TINY_ALLOC_MANAGER.tiny_zone_map, TINY_ALLOC_MANAGER.tiny_zone_mapped_size);
                AllocManager_uninit(TINY_MANAGER);
            }
        }
    }
    return (ret);
}

/**
 * @brief Resizes a memory block in the tiny zone.
 *
 * @details
 * Only allows shrinking within the same fixed-size slot. If the new size exceeds
 * TINY_ALLOC_SIZE, the operation fails, stores the old block info in the global
 * realloc helper for fallback, and sets *ptr to NULL to signal the caller to
 * perform a full reallocation. If the size fits, updates the metadata size.
 *
 * @param ptr  Pointer to the pointer to reallocate (modified on error).
 * @param size New requested size.
 * @return short 0 on success, negative on error.
 */
short ZoneAllocatorTiny_realloc(void **ptr, size_t size)
{
    short ret = -1;

    if (ptr == NULL)
    {
        *ptr = NULL; /* Invalid pointer */
    }
    else if (alloc_manager == NULL)
    {
        ret = -2;
    }
    else if (alloc_manager->tiny_set == 0)
    {
        ret = -2;
    }
    else if (*ptr < TINY_ALLOC_MANAGER.tiny_zone_start || *ptr > TINY_ALLOC_MANAGER.tiny_zone_end)
    {
        *ptr = NULL; /* Pointer out of range */
    }
    else if ((size == 0))
    {
        ret = -2;
        *ptr = NULL; /* Invalid size */
    }
    else
    {
        uint8_t index = ((uint8_t*)*ptr - (uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_start) / TINY_ALLOC_SIZE; /* Calculate the index of the block */
        if (index * TINY_ALLOC_SIZE + (uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_start != (uint8_t *)*ptr)
        {
            *ptr = NULL; /* Pointer not aligned */
        }
        else
        {
            ret = 0;
            if (size > TINY_ALLOC_SIZE)
            {
                alloc_manager->realloc_hlp.mem_size = ((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[index];
                alloc_manager->realloc_hlp.mem = *ptr;
                alloc_manager->realloc_hlp.manager = TINY_MANAGER;
                *ptr = NULL; /* Invalid size */
            }
            else
            {
                ((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[index] = size;
            }
        }
    }
    return (ret);
}

/**
 * @brief Prints a report of all allocated blocks in the tiny zone.
 *
 * @details
 * Outputs the base address of the mapped zone, then iterates through all slots.
 * For each allocated block (metadata != 0), prints the start address, end address
 * (start + stored size), and the stored size. Only executes if the zone is mapped.
 */
void ZoneAllocatorTiny_report(void)
{
    if (TINY_ALLOC_MANAGER.tiny_zone_map == NULL)
    {
        return;
    }
    write(1, "TINY : ", 6);
    print_address_as_hex(TINY_ALLOC_MANAGER.tiny_zone_map); /* Print the start address */
    write(1, "\n", 1);
    for (uint8_t i = 0u; i < TINY_ALLOC_COUNT; i++)
    {
        if (((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[i] != 0u)
        {
            print_address_as_hex((void *)((size_t)TINY_ALLOC_MANAGER.tiny_zone_start + (i * TINY_ALLOC_SIZE))); /* Print the address of the block */
            write(1, " - ", 3);
            print_address_as_hex((void *)(((size_t)TINY_ALLOC_MANAGER.tiny_zone_start + (i * TINY_ALLOC_SIZE) + ((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[i]))); /* Print the end address */
            write(1, " : ", 3);
            print_size(((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[i]); /* Print the size of the block */
            write(1, "\n", 1);
        }
    }
    write(1, "\n", 1);
}