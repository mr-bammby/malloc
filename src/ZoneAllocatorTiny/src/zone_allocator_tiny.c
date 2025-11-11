#include "zone_allocator_tiny.h"
#include "print_utils.h"
#include "alloc_manager.h"
#include <sys/mman.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>

#define TINY_ALLOC_COUNT 125u // Number of allocations
#define TINY_ZONE_SIZE (TINY_ALLOC_SIZE * TINY_ALLOC_COUNT) + TINY_ALLOC_COUNT  // Total size of the tiny zone
#define TINY_ALLOC_ALIGMENT 8u // Alignment of the tiny allocation

#define TINY_ALLOC_MANAGER alloc_manager->tiny_manager

// Allocates a block of memory of the given size.
void *ZoneAllocatorTiny_alloc(size_t size)
{
    if (size == 0)
    {
        return (0);
    }

    if (TINY_ALLOC_MANAGER.tiny_zone_map == NULL)
    {
        const size_t page_size = sysconf(_SC_PAGESIZE) ; // Get the page size

        if (AllocManager_init(TINY_MANAGER))
        {
            return NULL;
        }
        TINY_ALLOC_MANAGER.tiny_zone_mapped_size = TINY_ZONE_SIZE / page_size; // Calculate the number of aligned blocks
        TINY_ALLOC_MANAGER.tiny_zone_mapped_size = (TINY_ALLOC_MANAGER.tiny_zone_mapped_size * page_size) + ((TINY_ZONE_SIZE % page_size == 0u) ? (0u) : (page_size)); // Align to page size
        if (is_mmap_safe(TINY_ALLOC_MANAGER.tiny_zone_mapped_size) != 0)
        {
            AllocManager_uninit(TINY_MANAGER);
            return NULL;
        }
        TINY_ALLOC_MANAGER.tiny_zone_map = mmap(NULL, TINY_ALLOC_MANAGER.tiny_zone_mapped_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (TINY_ALLOC_MANAGER.tiny_zone_map == MAP_FAILED)
        {
            AllocManager_uninit(TINY_MANAGER);
            return NULL; // Allocation failed
        }
        uint8_t aligned_count = TINY_ALLOC_COUNT / TINY_ALLOC_ALIGMENT; // Calculate the number of aligned blocks
        aligned_count = (aligned_count * TINY_ALLOC_ALIGMENT) + ((TINY_ALLOC_COUNT % TINY_ALLOC_ALIGMENT == 0u) ? (0u) : (TINY_ALLOC_ALIGMENT)); // Align to 16
        TINY_ALLOC_MANAGER.tiny_zone_start = TINY_ALLOC_MANAGER.tiny_zone_map + aligned_count; // Set the start pointer
        TINY_ALLOC_MANAGER.tiny_zone_end = TINY_ALLOC_MANAGER.tiny_zone_start + TINY_ALLOC_SIZE * TINY_ALLOC_COUNT; // Set the end pointer
        TINY_ALLOC_MANAGER.tiny_alloc_cnt = 0u;
    }

    for(uint8_t i = 0u; i < TINY_ALLOC_COUNT; i++)
    {
        if ((((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[i]) == 0u)
        {
            ((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[i] = size; // Mark the block as used
            TINY_ALLOC_MANAGER.tiny_alloc_cnt++;
            return ((void *)((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_start + ((size_t)i * TINY_ALLOC_SIZE))); // Return the pointer to the allocated memory
        }
    }

    return (NULL);
}

size_t ZoneAllocatorTiny_size_get(void *ptr)
{
    size_t ret = 0;

    if (ptr == NULL)
    {
        ret = 0; // Invalid pointer
    }
    else if (ptr < TINY_ALLOC_MANAGER.tiny_zone_start || ptr > TINY_ALLOC_MANAGER.tiny_zone_end)
    {
        ret = 0; // Pointer out of range
    }
    else if (ptr == TINY_ALLOC_MANAGER.tiny_zone_map)
    {
        ret =  0; // Pointer is the map of the zone
    }
    else
    {
        uint8_t index = ((uint8_t*)ptr - (uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_start) / TINY_ALLOC_SIZE; // Calculate the index of the block
        if (index * TINY_ALLOC_SIZE + (uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_start != (uint8_t *)ptr)
        {
            ret = 0; // Pointer not aligned
        }
        else
        {
            ret = ((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[index]; // Return the size of the block
        }
    }
    return (ret);
}

// Frees the memory block pointed to by ptr.
short ZoneAllocatorTiny_free(void *ptr)
{
    short ret = 0;

    if (ptr == NULL)
    {
        ret = -1; // Invalid pointer
    }
    else if (ptr < TINY_ALLOC_MANAGER.tiny_zone_start || ptr > TINY_ALLOC_MANAGER.tiny_zone_end)
    {
        ret = -2; // Pointer out of range
    }
    else
    {
        uint8_t index = ((uint8_t*)ptr - (uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_start) / TINY_ALLOC_SIZE; // Calculate the index of the block
        if (index * TINY_ALLOC_SIZE + (uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_start != (uint8_t *)ptr)
        {
            ret = -2; // Pointer not aligned
        }
        else  // Free the block
        {
            ((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[index] = 0u; // Mark the block as free
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

// This function only performs a realocation if the pointer is valid and the size is valid for the tiny zone.
// It checks if the pointer is in the tiny zone.
short ZoneAllocatorTiny_realloc(void **ptr, size_t size)
{
    short ret = -1;

    if (ptr == NULL)
    {
        *ptr = NULL; // Invalid pointer
    }
    else if (*ptr < TINY_ALLOC_MANAGER.tiny_zone_start || *ptr > TINY_ALLOC_MANAGER.tiny_zone_end)
    {
        *ptr = NULL; // Pointer out of range
    }
    else if ((size == 0) || (size > TINY_ALLOC_SIZE))
    {
        ZoneAllocatorTiny_free(*ptr);
        ret = -3;
        *ptr =  NULL; // Invalid size
    }
    else
    {
        uint8_t index = ((uint8_t*)*ptr - (uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_start) / TINY_ALLOC_SIZE; // Calculate the index of the block
        if (index * TINY_ALLOC_SIZE + (uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_start != (uint8_t *)*ptr)
        {
            *ptr = NULL; // Pointer not aligned
        }
        else 
        {
            ret = 0;
            ((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[index] = size;
        }
    }
    return (ret);
}

// This function prints the memory map of the tiny zone.
// It prints the start address, end address, and size of each block.
// It also prints the start address of the tiny zone.
void ZoneAllocatorTiny_report(void)
{
    if (TINY_ALLOC_MANAGER.tiny_zone_map == NULL)
    {
        return;
    }
    write (1, "TINY : ", 6);
    print_address_as_hex(TINY_ALLOC_MANAGER.tiny_zone_map); // Print the start address
    write (1, "\n", 1);
    for (uint8_t i = 0u; i < TINY_ALLOC_COUNT; i++)
    {
        if (((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[i] != 0u)
        {
            print_address_as_hex((void *)((size_t)TINY_ALLOC_MANAGER.tiny_zone_start + (i * TINY_ALLOC_SIZE))); // Print the address of the block
            write (1, " - ", 3);
            print_address_as_hex((void *)(((size_t)TINY_ALLOC_MANAGER.tiny_zone_start + (i * TINY_ALLOC_SIZE) + ((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[i]))); // Print the end address
            write (1, " : ", 3);
            print_size(((uint8_t *)TINY_ALLOC_MANAGER.tiny_zone_map)[i]); // Print the size of the block
            write (1, "\n", 1);
        }
    }
}
