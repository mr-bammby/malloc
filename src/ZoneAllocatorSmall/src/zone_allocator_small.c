#include "zone_allocator_small.h"
#include "print_utils.h"
#include "alloc_manager.h"
#include <sys/mman.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>

#define SMALL_BLOCK_HEADER_SIZE sizeof(small_block_header_t) // Size of the header
#define SMALL_MAP_HEADER_SIZE sizeof(small_map_header_t) // Size of the header
#define SMALL_MAP_DEFAULT_ALLOC 	16ul //in pages

#define SMALL_ALLOC_ALIGMENT 8u // Alignment of the small allocation
#define SMALL_ALLOC_NUM 125u

#define SMALL_ALLOC_MANAGER alloc_manager->small_manager

static size_t new_map_add(small_map_header_t **new_map, size_t size)
{
	size_t aligned_size;
	const size_t page_size = sysconf(_SC_PAGESIZE);
	size_t map_size = page_size * SMALL_MAP_DEFAULT_ALLOC; // Get the page size

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
	if (new_map == MAP_FAILED)
	{
		AllocManager_uninit(SMALL_MANAGER);
		return 0u;
	}
	(*new_map)-> next = NULL;
	(*new_map)-> cnt = 1;

	(*new_map)-> size = map_size;
	SMALL_ALLOC_MANAGER.small_zone_end = *new_map;

	aligned_size = SMALL_MAP_HEADER_SIZE / SMALL_ALLOC_ALIGMENT;
	aligned_size = (SMALL_MAP_HEADER_SIZE % SMALL_ALLOC_ALIGMENT == 0) ? (aligned_size) : (aligned_size + 1);
	aligned_size *= SMALL_ALLOC_ALIGMENT;
	SMALL_ALLOC_MANAGER.small_zone_end->first_block = (small_block_header_t *)((uint8_t *)SMALL_ALLOC_MANAGER.small_zone_end + aligned_size);

	return (map_size - aligned_size);
}

static void *new_map_alloc(size_t size, size_t free_map_size)
{
	size_t full_block_size;

	SMALL_ALLOC_MANAGER.small_zone_end->first_block->used = size;
	full_block_size = size + SMALL_BLOCK_HEADER_SIZE;
	SMALL_ALLOC_MANAGER.small_zone_end->first_block->size  = full_block_size / SMALL_ALLOC_ALIGMENT;
	SMALL_ALLOC_MANAGER.small_zone_end->first_block->size  = (full_block_size % SMALL_ALLOC_ALIGMENT == 0) ? (SMALL_ALLOC_MANAGER.small_zone_end->first_block->size) : (SMALL_ALLOC_MANAGER.small_zone_end->first_block->size + 1);
	SMALL_ALLOC_MANAGER.small_zone_end->first_block->size  *= SMALL_ALLOC_ALIGMENT;
	SMALL_ALLOC_MANAGER.small_zone_end->first_block->size -= SMALL_BLOCK_HEADER_SIZE;
	if ((free_map_size - SMALL_ALLOC_MANAGER.small_zone_end->first_block->size) > (2 * SMALL_BLOCK_HEADER_SIZE))
	{
		SMALL_ALLOC_MANAGER.small_zone_end->first_block->next = (small_block_header_t *)((uint8_t *)SMALL_ALLOC_MANAGER.small_zone_end->first_block + SMALL_ALLOC_MANAGER.small_zone_end->first_block->size + SMALL_BLOCK_HEADER_SIZE);
		SMALL_ALLOC_MANAGER.small_zone_end->first_block->next->next = NULL;
		free_map_size -= (SMALL_ALLOC_MANAGER.small_zone_end->first_block->size + SMALL_BLOCK_HEADER_SIZE);
		SMALL_ALLOC_MANAGER.small_zone_end->first_block->next->size = free_map_size - SMALL_BLOCK_HEADER_SIZE;
		SMALL_ALLOC_MANAGER.small_zone_end->first_block->next->used = 0;
	}
	else
	{
		SMALL_ALLOC_MANAGER.small_zone_end->first_block->next = NULL;
	}
	SMALL_ALLOC_MANAGER.small_alloc_cnt++;

	return ((uint8_t *)SMALL_ALLOC_MANAGER.small_zone_end->first_block + SMALL_BLOCK_HEADER_SIZE);
}

static void *old_map_alloc(size_t size)
{
    small_map_header_t *current_map = SMALL_ALLOC_MANAGER.small_zone_start;
    small_block_header_t *current_block, *new_block;
    size_t full_size, aligned_size, required, diff;
    void *ret = NULL;

    full_size = size + SMALL_BLOCK_HEADER_SIZE;
    aligned_size = full_size / SMALL_ALLOC_ALIGMENT;
    aligned_size = (full_size % SMALL_ALLOC_ALIGMENT == 0) ? aligned_size : (aligned_size + 1);
    aligned_size *= SMALL_ALLOC_ALIGMENT;  // A
    required = aligned_size - SMALL_BLOCK_HEADER_SIZE;  // Needed data size

    while (current_map != NULL)
    {
        current_block = current_map->first_block;
        while (current_block != NULL)
        {
            if ((current_block->used == 0) && (current_block->size >= required))
            {
                current_block->used = size;
                diff = current_block->size - required;
                if (diff > 2 * SMALL_BLOCK_HEADER_SIZE)
                {
                    new_block = (void *)((uint8_t *)current_block + aligned_size);
                    new_block->next = current_block->next;
                    current_block->next = new_block;
                    new_block->size = diff - SMALL_BLOCK_HEADER_SIZE;
                    new_block->used = 0;
                    current_block->size = required;
                }
                ret = (void *)current_block + SMALL_BLOCK_HEADER_SIZE;
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

// Allocates a block of memory of the given size.
void *ZoneAllocatorSmall_alloc(size_t size)
{
	size_t free_map_size;
	void *ret = NULL;

	if ((size == 0) || (SMALL_ALLOC_MANAGER.small_alloc_cnt > SMALL_ALLOC_NUM))
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

size_t ZoneAllocatorSmall_size_get(void *ptr)
{
    size_t ret = 0;

    if (ptr == NULL)
    {
        ret = 0; // Invalid pointer
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
				if (((void *)current_block + SMALL_BLOCK_HEADER_SIZE) == ptr)
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
				size += SMALL_BLOCK_HEADER_SIZE;
				temp_block = next_block->next;
			}
		}
		if (prev_block != NULL)
		{
			if (prev_block->used == 0u)
			{
				size += prev_block->size;
				size += SMALL_BLOCK_HEADER_SIZE;
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

// Frees the memory block pointed to by ptr.
short ZoneAllocatorSmall_free(void *ptr)
{
    short ret = 0;
    if (ptr == NULL)
    {
        ret = -1; // Invalid pointer
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
				if (((void *)current_block + SMALL_BLOCK_HEADER_SIZE) == ptr)
				{
					current_block->used = 0; //Freed
					current_map->cnt--;
					SMALL_ALLOC_MANAGER.small_alloc_cnt--;
					if(current_map->cnt == 0)
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
			ret = -2; //Not found
		}
    }
    return (ret);
}

// This function only performs a realocation if the pointer is valid and the size is valid for the small zone.
// It checks if the pointer is in the small zone.
short ZoneAllocatorSmall_realloc(void *ptr, size_t size)
{
	short ret = 0;

	if (ptr == NULL)
	{
		ret = -1; // Invalid pointer
	}
	else if (size == 0)
	{
		ret = ZoneAllocatorSmall_free(ptr);
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
				if (((void *)current_block + SMALL_BLOCK_HEADER_SIZE) == ptr)
				{
					size_t aligned_size = (size + SMALL_BLOCK_HEADER_SIZE) / SMALL_ALLOC_ALIGMENT; // Calculate the aligned size
					aligned_size = aligned_size * SMALL_ALLOC_ALIGMENT; // Align the size
					aligned_size += (aligned_size % SMALL_ALLOC_ALIGMENT == 0u) ? (0u) : (SMALL_ALLOC_ALIGMENT); // Align to 16
					small_block_header_t* next_block = current_block->next;
					if (current_block->size < size)
					{
						if ((next_block != NULL)) 
						{
							size_t max_size = current_block->size + next_block->size + 2 * SMALL_BLOCK_HEADER_SIZE;
							if (max_size > aligned_size)
							{
								current_block->used = size;
								size_t size_diff = max_size  - aligned_size;
								if (size_diff > (2 * SMALL_BLOCK_HEADER_SIZE))
								{
									current_block->next = (void *)((uint8_t *)current_block + aligned_size);
									current_block->next->next = next_block->next;
									current_block->next->used = 0;
									current_block->next->size = size_diff - SMALL_BLOCK_HEADER_SIZE;
								}
								else
								{
									current_block->next = next_block->next;
								}
								
								current_block->size = aligned_size - SMALL_BLOCK_HEADER_SIZE;
							}
							else
							{
								current_block->used = 0; //Freed
								current_map->cnt--;
								SMALL_ALLOC_MANAGER.small_alloc_cnt--;
								if(current_map->cnt == 0)
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
								ptr = ZoneAllocatorSmall_alloc(size);
							}

						}
						else
						{
							current_block->used = 0; //Freed
							current_map->cnt--;
							SMALL_ALLOC_MANAGER.small_alloc_cnt--;
							if(current_map->cnt == 0)
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
							ptr = ZoneAllocatorSmall_alloc(size);
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
		if (current_block == NULL)
		{
			ptr = ZoneAllocatorSmall_alloc(size);
		}
    }
	return ret;
}


// This function prints the memory map of the big zone.
// It prints the start address, end address, and size of each block.
// It also prints the start address of each map.
void ZoneAllocatorBig_report(void)
{
    if (SMALL_ALLOC_MANAGER.small_zone_start == NULL)
    {
        return;
    }
    write (1, "SMALL : ", 7);
	small_map_header_t *current_map = SMALL_ALLOC_MANAGER.small_zone_start;
	small_block_header_t *current_block;
	while  (current_map != NULL)
	{
		print_address_as_hex((void *)current_map); // Print the start address of map
		write (1, "\n", 1);

		current_block = current_map->first_block; // Set the current block
		while (current_block != NULL)
		{
			if (current_block->used != 0u)
			{
				print_address_as_hex((void *)((uint8_t *)current_block + SMALL_BLOCK_HEADER_SIZE)); //Print the address of the block
				write (1, " - ", 3);
				print_address_as_hex((void *)((uint8_t *)current_block + SMALL_BLOCK_HEADER_SIZE + current_block->used)); //Print the end address
				write (1, " : ", 3);
				print_size(current_block->used); // Print the size of the block
				write (1, "\n", 1);
			}
			current_block = current_block->next; // Move to the next block
		}
		write (1, "\n", 1);
		current_map = current_map->next;
	}
}
