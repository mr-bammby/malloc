#include "zone_allocator_large.h"
#include "print_utils.h"
#include "alloc_manager.h"
#include <sys/mman.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>

#define LARGE_BLOCK_HEADER_SIZE sizeof(large_block_header_t) // Size of the header
#define LARGE_MAP_HEADER_SIZE sizeof(large_map_header_t) // Size of the header

#define LARGE_ALLOC_ALIGMENT 8u // Alignment of the large allocation
#define LARGE_ALLOC_NUM 125u

#define LARGE_ALLOC_MANAGER alloc_manager->large_manager


static size_t new_map_add(large_map_header_t **new_map, size_t size)
{
	size_t aligned_size, map_size = 0;
	const int page_size = sysconf(_SC_PAGESIZE) ; // Get the page size
	const size_t full_size = size + LARGE_BLOCK_HEADER_SIZE + LARGE_MAP_HEADER_SIZE;

	if (AllocManager_init(LARGE_MANAGER) != 0)
	{
		return 0u;
	}
	map_size = full_size / page_size;
	map_size = ((full_size % page_size) == 0) ? (map_size) : (map_size + 1);
	map_size *= page_size;

	if (is_mmap_safe(map_size) != 0)
	{
		AllocManager_uninit(LARGE_MANAGER);
		return 0u;
	}
	*new_map = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (*new_map == MAP_FAILED)
	{
		AllocManager_uninit(LARGE_MANAGER);
		return 0u;
	}
	(*new_map)-> next = NULL;
	(*new_map)-> cnt = 1;
	(*new_map)-> size = map_size;
	LARGE_ALLOC_MANAGER.large_zone_end = *new_map;

	aligned_size = LARGE_MAP_HEADER_SIZE / LARGE_ALLOC_ALIGMENT;
	aligned_size = (LARGE_MAP_HEADER_SIZE % LARGE_ALLOC_ALIGMENT == 0) ? (aligned_size) : (aligned_size + 1);
	aligned_size *= LARGE_ALLOC_ALIGMENT;
	LARGE_ALLOC_MANAGER.large_zone_end->first_block = (large_block_header_t *)((uint8_t *)LARGE_ALLOC_MANAGER.large_zone_end + aligned_size);

	return (map_size - aligned_size);
}

static void *new_map_alloc(size_t size, size_t free_map_size)
{
	size_t full_block_size;

	LARGE_ALLOC_MANAGER.large_zone_end->first_block->used = size;
	full_block_size = size + LARGE_BLOCK_HEADER_SIZE;
	LARGE_ALLOC_MANAGER.large_zone_end->first_block->size  = full_block_size / LARGE_ALLOC_ALIGMENT;
	LARGE_ALLOC_MANAGER.large_zone_end->first_block->size  = (full_block_size % LARGE_ALLOC_ALIGMENT == 0) ? (LARGE_ALLOC_MANAGER.large_zone_end->first_block->size) : (LARGE_ALLOC_MANAGER.large_zone_end->first_block->size + 1);
	LARGE_ALLOC_MANAGER.large_zone_end->first_block->size  *= LARGE_ALLOC_ALIGMENT;
	LARGE_ALLOC_MANAGER.large_zone_end->first_block->size -= LARGE_BLOCK_HEADER_SIZE;
	if ((free_map_size - LARGE_ALLOC_MANAGER.large_zone_end->first_block->size) > (2 * LARGE_BLOCK_HEADER_SIZE))
	{
		LARGE_ALLOC_MANAGER.large_zone_end->first_block->next = (large_block_header_t *)((uint8_t *)LARGE_ALLOC_MANAGER.large_zone_end->first_block + LARGE_ALLOC_MANAGER.large_zone_end->first_block->size + LARGE_BLOCK_HEADER_SIZE);
		LARGE_ALLOC_MANAGER.large_zone_end->first_block->next->next = NULL;
		free_map_size -= (LARGE_ALLOC_MANAGER.large_zone_end->first_block->size + LARGE_BLOCK_HEADER_SIZE);
		LARGE_ALLOC_MANAGER.large_zone_end->first_block->next->size = free_map_size - LARGE_BLOCK_HEADER_SIZE;
		LARGE_ALLOC_MANAGER.large_zone_end->first_block->next->used = 0;
	}
	else
	{
		LARGE_ALLOC_MANAGER.large_zone_end->first_block->next = NULL;
	}
	LARGE_ALLOC_MANAGER.large_alloc_cnt++;

	return ((uint8_t *)LARGE_ALLOC_MANAGER.large_zone_end->first_block + LARGE_BLOCK_HEADER_SIZE);
}

// Allocates a block of memory of the given size.
void *ZoneAllocatorLarge_alloc(size_t size)
{
	size_t free_map_size;
	void * ret = NULL;

	if ((size == 0) || (LARGE_ALLOC_MANAGER.large_alloc_cnt > LARGE_ALLOC_NUM))
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

size_t ZoneAllocatorLarge_size_get(void *ptr)
{
    size_t ret = 0;

    if (ptr == NULL)
    {
        ret = 0; // Invalid pointer
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
				if (((void *)current_block + LARGE_BLOCK_HEADER_SIZE) == ptr)
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

static void defrag(large_block_header_t* prev_block, large_block_header_t *block)
{
	if (block != NULL)
	{
		large_block_header_t* next_block = block->next;
		size_t size = block->size;
		large_block_header_t* temp_block = block->next;

		if (next_block != NULL)
		{
			if (next_block->used == 0)
			{
				size += next_block->size;
				size += LARGE_BLOCK_HEADER_SIZE;
				temp_block = next_block->next;
			}
		}
		if (prev_block != NULL)
		{
			if (prev_block->used == 0)
			{
				size += prev_block->size;
				size += LARGE_BLOCK_HEADER_SIZE;
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
short ZoneAllocatorLarge_free(void *ptr)
{
    short ret = 0;
    if (ptr == NULL)
    {
        ret = -1; // Invalid pointer
    }
	else
    {
		large_map_header_t *current_map = LARGE_ALLOC_MANAGER.large_zone_start;
		large_map_header_t *prev_map = NULL;
		large_block_header_t *current_block, *prev_block;

		while (current_map != NULL)
		{
			prev_block = NULL;
			current_block = current_map->first_block;
			while (current_block != NULL)
			{
				if (((void *)current_block + LARGE_BLOCK_HEADER_SIZE) == ptr)
				{
					current_block->used = 0; //Freed
					current_map->cnt--;
					LARGE_ALLOC_MANAGER.large_alloc_cnt--;
					if(current_map->cnt == 0)
					{	
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
						munmap((void *)current_map, current_map->size);
						AllocManager_uninit(LARGE_MANAGER);
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

// This function only performs a realocation if the pointer is valid and the size is valid for the large zone.
// It checks if the pointer is in the large zone.
short ZoneAllocatorLarge_realloc(void *ptr, size_t size)
{
	short ret = 0;

	if (ptr == NULL)
	{
		ret = -1; // Invalid pointer
	}
	else if (size == 0)
	{
		ret = ZoneAllocatorLarge_free(ptr);
	}
	else
	{
		large_map_header_t *current_map = LARGE_ALLOC_MANAGER.large_zone_start;
		large_map_header_t *prev_map = NULL;
		large_block_header_t *current_block, *prev_block;

		while (current_map != NULL)
		{
			prev_block = NULL;
			current_block = current_map->first_block;
			while (current_block != NULL)
			{
				if (((void *)current_block + LARGE_BLOCK_HEADER_SIZE) == ptr)
				{
					size_t aligned_size = (size + LARGE_BLOCK_HEADER_SIZE) / LARGE_ALLOC_ALIGMENT; // Calculate the aligned size
					aligned_size = aligned_size * LARGE_ALLOC_ALIGMENT; // Align the size
					aligned_size += (aligned_size % LARGE_ALLOC_ALIGMENT == 0u) ? (0u) : (LARGE_ALLOC_ALIGMENT); // Align to 16
					large_block_header_t* next_block = current_block->next;
					if (current_block->size < size)
					{
						if ((next_block != NULL)) 
						{
							size_t max_size = current_block->size + next_block->size + 2 * LARGE_BLOCK_HEADER_SIZE;
							if (max_size > aligned_size)
							{
								current_block->used = size;
								size_t size_diff = max_size  - aligned_size;
								if (size_diff > (2 * LARGE_BLOCK_HEADER_SIZE))
								{
									current_block->next = (void *)((uint8_t *)current_block + aligned_size);
									current_block->next->next = next_block->next;
									current_block->next->used = 0;
									current_block->next->size = size_diff - LARGE_BLOCK_HEADER_SIZE;
								}
								else
								{
									current_block->next = next_block->next;
								}
								
								current_block->size = aligned_size - LARGE_BLOCK_HEADER_SIZE;
							}
							else
							{
								current_block->used = 0; //Freed
								current_map->cnt--;
								LARGE_ALLOC_MANAGER.large_alloc_cnt--;
								if(current_map->cnt == 0)
								{
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
									munmap((void *)current_map, current_map->size);
									AllocManager_uninit(LARGE_MANAGER);
								}
								else
								{
									defrag(prev_block, current_block);
								}
								ptr = ZoneAllocatorLarge_alloc(size);
							}

						}
						else
						{
							current_block->used = 0; //Freed
							current_map->cnt--;
							LARGE_ALLOC_MANAGER.large_alloc_cnt--;
							if(current_map->cnt == 0)
							{
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
								munmap((void *)current_map, current_map->size);
								AllocManager_uninit(LARGE_MANAGER);
							}
							else
							{
								defrag(prev_block, current_block);
							}
							ptr = ZoneAllocatorLarge_alloc(size);
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
			ptr = ZoneAllocatorLarge_alloc(size);
		}
    }
	return ret;
}


// This function prints the memory map of the large zone.
// It prints the start address, end address, and size of each block.
// It also prints the start address of each map.
void ZoneAllocatorLarge_report(void)
{
    if (LARGE_ALLOC_MANAGER.large_zone_start == NULL)
    {
        return;
    }
    write (1, "LARGE : ", 7);
	large_map_header_t *current_map = LARGE_ALLOC_MANAGER.large_zone_start;
	large_block_header_t *current_block;
	while  (current_map != NULL)
	{
		print_address_as_hex((void *)current_map); // Print the start address of map
		write (1, "\n", 1);

		current_block = current_map->first_block; // Set the current block
		while (current_block != NULL)
		{
			if (current_block->used != 0u)
			{
				print_address_as_hex((void *)((uint8_t *)current_block + LARGE_BLOCK_HEADER_SIZE)); //Print the address of the block
				write (1, " - ", 3);
				print_address_as_hex((void *)((uint8_t *)current_block + LARGE_BLOCK_HEADER_SIZE + current_block->used)); //Print the end address
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
