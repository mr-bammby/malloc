#include "../inc_pub/zone_allocator_big.h"
#include "../../print_utils/inc_pub/print_utils.h"
#include <sys/mman.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>

typedef struct big_block_header big_block_header_t;
typedef struct big_map_header big_map_header_t;

struct big_block_header
{
    size_t size; // Size of the block
    size_t used; // Used size of the block
    big_block_header_t *next; // Pointer to the next block
};

struct big_map_header
{
    big_block_header_t *first_block; 	// Size of the block
    size_t cnt; 						// Used for mumap
	size_t size;						// Used for mumap
    big_map_header_t *next; 			// Pointer to the next map
};

#define BIG_BLOCK_HEADER_SIZE sizeof(big_block_header_t) // Size of the header
#define BIG_MAP_HEADER_SIZE sizeof(big_map_header_t) // Size of the header
#define BIG_MAP_MIN_ALLOC 		10u //in pages
#define BIG_MAP_DEFAULT_ALLOC 	20u //in pages

#define BIG_ALLOC_ALIGMENT 16u // Alignment of the small allocation


big_map_header_t *big_zone_start = NULL; // Pointer to the small zone
big_map_header_t *big_zone_end = NULL; // Pointer to the small zone

static size_t new_map_add(big_map_header_t **new_map, size_t map_size)
{
	size_t aligned_size;

	*new_map = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	(*new_map)-> next = NULL;
	(*new_map)-> cnt = 1;
	(*new_map)-> size = map_size;
	big_zone_end = *new_map;

	aligned_size = BIG_MAP_HEADER_SIZE / BIG_ALLOC_ALIGMENT;
	aligned_size = (BIG_MAP_HEADER_SIZE % BIG_ALLOC_ALIGMENT == 0) ? (aligned_size) : (aligned_size + 1);
	aligned_size *= BIG_ALLOC_ALIGMENT;
	big_zone_end->first_block = (void *)big_zone_end + aligned_size;

	return (aligned_size);
}

static void *new_map_alloc(size_t size, size_t map_size, size_t free_map_size)
{
	size_t full_block_size;

	big_zone_end->first_block->used = size;
	full_block_size = size + BIG_BLOCK_HEADER_SIZE;
	big_zone_end->first_block->size  = full_block_size / BIG_ALLOC_ALIGMENT;
	big_zone_end->first_block->size  = (full_block_size % BIG_ALLOC_ALIGMENT == 0) ? (big_zone_end->first_block->size) : (big_zone_end->first_block->size + 1);
	big_zone_end->first_block->size  *= BIG_ALLOC_ALIGMENT;
	big_zone_end->first_block->size -= BIG_BLOCK_HEADER_SIZE;
	if ((free_map_size - big_zone_end->first_block->size) > (2 * BIG_BLOCK_HEADER_SIZE))
	{
		big_zone_end->first_block->next = (big_block_header_t *)((uint8_t *)big_zone_end->first_block + big_zone_end->first_block->size + BIG_BLOCK_HEADER_SIZE);
		big_zone_end->first_block->next->next = NULL;
		free_map_size -= (big_zone_end->first_block->size + BIG_BLOCK_HEADER_SIZE);
		big_zone_end->first_block->next->size = free_map_size - BIG_BLOCK_HEADER_SIZE;
		big_zone_end->first_block->next->used = 0;
	}
	else
	{
		big_zone_end->first_block->next = NULL;
	}

	return ((void *)big_zone_end->first_block + BIG_BLOCK_HEADER_SIZE);
}

static void *old_map_alloc(size_t size)
{
	big_map_header_t *current_map = big_zone_start;
	big_block_header_t *current_block, *new_block;
	size_t aligned_size, full_size;
	void *ret = NULL;

	while (current_map != NULL)
	{
		current_block = current_map->first_block;
		while (current_block != NULL)
		{
			if ((current_block->used == 0) && (current_block->size >= size))
			{
				current_block->used = size;
				full_size = size + BIG_BLOCK_HEADER_SIZE;
				aligned_size = full_size / BIG_ALLOC_ALIGMENT;
				aligned_size = (full_size % BIG_ALLOC_ALIGMENT == 0) ? (aligned_size) : (aligned_size + 1);
				aligned_size *= BIG_ALLOC_ALIGMENT;
				if ((current_block->size - aligned_size) > (BIG_BLOCK_HEADER_SIZE))
				{
					new_block = (void *)current_block + aligned_size;
					new_block->next = current_block->next;
					current_block->next = new_block;
					new_block->size = current_block->size - (aligned_size + BIG_BLOCK_HEADER_SIZE);
					new_block->used = 0;
					current_block->size = aligned_size - BIG_BLOCK_HEADER_SIZE;
				}
				ret = (void *)current_block + BIG_BLOCK_HEADER_SIZE;
				current_map->cnt++;
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
void *ZoneAllocatorBig_alloc(size_t size)
{
    big_map_header_t *start_header;
	const int page_size = sysconf(_SC_PAGESIZE) ; // Get the page size
	const size_t full_size = size + BIG_BLOCK_HEADER_SIZE + BIG_MAP_HEADER_SIZE;
	size_t aligned_size, map_size = 0;
	void * ret = NULL;

	if (size == 0)
	{
		return (ret);
	}

	if (full_size > (BIG_MAP_MIN_ALLOC * page_size))
	{
		map_size = full_size / page_size;
		map_size = ((full_size % page_size) == 0) ? (map_size) : (map_size + 1);
		map_size *= page_size;
	}
	else
	{
		ret = old_map_alloc(size);
		if (ret == NULL)
		{
			map_size = BIG_MAP_DEFAULT_ALLOC * page_size;
		}
	}
	
	if (map_size != 0)
	{
		if (big_zone_start == NULL) 
		{
			aligned_size = new_map_add(&big_zone_start, map_size);
		}
		else
		{
			aligned_size = new_map_add(&(big_zone_end->next), map_size);
		}
		ret = new_map_alloc(size, map_size, map_size - aligned_size);	
	}

    return (ret);
}

size_t ZoneAllocatorBig_size_get(void *ptr)
{
    size_t ret = 0;

    if (ptr == NULL)
    {
        ret = 0; // Invalid pointer
    }
    else
    {
		big_map_header_t *current_map = big_zone_start;
		big_block_header_t *current_block;

		while (current_map != NULL)
		{
			current_block = current_map->first_block;
			while (current_block != NULL)
			{
				if (((void *)current_block + BIG_BLOCK_HEADER_SIZE) == ptr)
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

static void defrag(big_block_header_t* prev_block, big_block_header_t *block)
{
	if (block != NULL)
	{
		big_block_header_t* next_block = block->next;
		size_t size = block->size;
		big_block_header_t* temp_block = block->next;

		if (next_block != NULL)
		{
			if (next_block->used == 0)
			{
				size += next_block->used;
				size += BIG_BLOCK_HEADER_SIZE;
				temp_block = next_block->next;
			}
		}
		if (prev_block != NULL)
		{
			if (prev_block->used == 0);
			{
				size += prev_block->size;
				size += BIG_BLOCK_HEADER_SIZE;
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
short ZoneAllocatorBig_free(void *ptr)
{
    short ret = 0;
    if (ptr == NULL)
    {
        ret = -1; // Invalid pointer
    }
	else
    {
		big_map_header_t *current_map = big_zone_start;
		big_map_header_t *prev_map = NULL;
		big_block_header_t *current_block, *prev_block;

		while (current_map != NULL)
		{
			prev_block = NULL;
			current_block = current_map->first_block;
			while (current_block != NULL)
			{
				if (((void *)current_block + BIG_BLOCK_HEADER_SIZE) == ptr)
				{
					current_block->used = 0; //Freed
					current_map->cnt--;
					if(current_map->cnt == 0)
					{	
						if (prev_map == NULL)
						{
							big_zone_start = current_map->next;
						}
						else
						{
							prev_map->next = current_map->next;
						}
						if (current_map->next == NULL)
						{
							big_zone_end = prev_map;
						}
						munmap((void *)current_map, current_map->size);
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
void *ZoneAllocatorBig_realloc(void *ptr, size_t size)
{
	void *ret = ptr;

	if (ptr == NULL)
	{
		ret = NULL; // Invalid pointer
	}
	else if (size == 0)
	{
		ZoneAllocatorBig_free(ptr);
		ret = NULL; // Invalid size
	}
	else
	{
		big_map_header_t *current_map = big_zone_start;
		big_map_header_t *prev_map = NULL;
		big_block_header_t *current_block, *prev_block;

		while (current_map != NULL)
		{
			prev_block = NULL;
			current_block = current_map->first_block;
			while (current_block != NULL)
			{
				if (((void *)current_block + BIG_BLOCK_HEADER_SIZE) == ptr)
				{
					size_t aligned_size = (size + BIG_BLOCK_HEADER_SIZE) / BIG_ALLOC_ALIGMENT; // Calculate the aligned size
					aligned_size = aligned_size * BIG_ALLOC_ALIGMENT; // Align the size
					aligned_size += (aligned_size % BIG_ALLOC_ALIGMENT == 0u) ? (0u) : (BIG_ALLOC_ALIGMENT); // Align to 16
					aligned_size -= BIG_BLOCK_HEADER_SIZE;
					big_block_header_t* next_block = current_block->next;
					if (current_block->size < aligned_size)
					{
						if ((next_block != NULL)) 
						{
							size_t max_size = current_block->size + next_block->size + BIG_BLOCK_HEADER_SIZE;
							if (max_size > aligned_size)
							{
								current_block->next = (void *)current_block + aligned_size;
								current_block->next->next = next_block->next;
								current_block->next->used = 0;
								current_block->next->size = max_size - aligned_size - BIG_BLOCK_HEADER_SIZE;
								current_block->used = size;
								current_block->size = aligned_size;
							}
							else if (max_size == aligned_size)
							{	
								current_block->next = next_block->next;
								current_block->used = size;
								current_block->size = aligned_size;
							}
							else
							{
								current_block->used = 0; //Freed
								current_map->cnt--;
								if(current_map->cnt == 0)
								{
									if (prev_map == NULL)
									{
										big_zone_start = current_map->next;
									}
									else
									{
										prev_map->next = current_map->next;
									}
									if (current_map->next == NULL)
									{
										big_zone_end = prev_map;
									}
									munmap((void *)current_map, current_map->size);
								}
								else
								{
									defrag(prev_block, current_block);
								}
								ret = ZoneAllocatorBig_alloc(size);
							}

						}
						else
						{
							current_block->used = 0; //Freed
							current_map->cnt--;
							if(current_map->cnt == 0)
							{
								if (prev_map == NULL)
								{
									big_zone_start = current_map->next;
								}
								else
								{
									prev_map->next = current_map->next;
								}
								if (current_map->next == NULL)
								{
									big_zone_end = prev_map;
								}
								munmap((void *)current_map, current_map->size);
							}
							else
							{
								defrag(prev_block, current_block);
							}
							ret = ZoneAllocatorBig_alloc(size);
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
			ret = ZoneAllocatorBig_alloc(size);
		}
    }
	return ret;
}


// This function prints the memory map of the big zone.
// It prints the start address, end address, and size of each block.
// It also prints the start address of each map.
void ZoneAllocatorBig_report(void)
{
    if (big_zone_start == NULL)
    {
        return;
    }
    write (1, "BIG : ", 6);
	big_map_header_t *current_map = big_zone_start;
	big_block_header_t *current_block;
	while  (current_map != NULL)
	{
		print_address_as_hex((void *)current_map); // Print the start address of map
		write (1, "\n", 1);

		current_block = current_map->first_block; // Set the current block
		while (current_block != NULL)
		{
			if (current_block->used != 0u)
			{
				print_address_as_hex((void *)((uint8_t *)current_block + BIG_BLOCK_HEADER_SIZE)); //Print the address of the block
				write (1, " - ", 3);
				print_address_as_hex((void *)((uint8_t *)current_block + BIG_BLOCK_HEADER_SIZE + current_block->used)); //Print the end address
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
