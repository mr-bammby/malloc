#ifndef IG_ALLOC_MANAGER
#define IG_ALLOC_MANAGER

#include <stddef.h>
#include <inttypes.h>

#ifdef FT_BONUS
#include <pthread.h>
#endif /* FT_BONUS */

typedef enum
{
    TINY_MANAGER = 1,
    SMALL_MANAGER = 2,
    LARGE_MANAGER = 3
} alloc_manager_e;

typedef struct small_block_header small_block_header_t;
typedef struct small_map_header small_map_header_t;

struct small_block_header
{
    size_t size; // Size of the block
    size_t used; // Used size of the block
    small_block_header_t *next; // Pointer to the next block
};

struct small_map_header
{
    small_block_header_t *first_block; 	// Size of the block
    size_t cnt; 						// Used for mumap
	size_t size;						// Used for mumap
	small_map_header_t *next; 			// Pointer to the next map
};

typedef struct large_block_header large_block_header_t;
typedef struct large_map_header large_map_header_t;

struct large_block_header
{
    size_t size; // Size of the block
    size_t used; // Used size of the block
    large_block_header_t *next; // Pointer to the next block
};

struct large_map_header
{
    large_block_header_t *first_block; 	// Size of the block
    size_t cnt; 						// Used for mumap
	size_t size;						// Used for mumap
    large_map_header_t *next; 			// Pointer to the next map
};

typedef struct tiny_manager_s
{
    void *tiny_zone_map;    // Pointer to the tiny zone
    void *tiny_zone_start;  // Pointer to the first tiny zone alloc
    void *tiny_zone_end;    // Pointer to the after tiny zone
    size_t tiny_zone_mapped_size;
    size_t tiny_alloc_cnt;
} tiny_manager_t;

typedef struct small_manager_s
{
    small_map_header_t *small_zone_start; // Pointer to the small zone
    small_map_header_t *small_zone_end;   // Pointer to the small zone
    size_t small_alloc_cnt;
} small_manager_t;

typedef struct large_manager_s
{
    large_map_header_t *large_zone_start; // Pointer to the large zone
    large_map_header_t *large_zone_end; // Pointer to the large zone
    size_t large_alloc_cnt;
} large_manager_t;


typedef struct alloc_manager_s
{
    tiny_manager_t tiny_manager;
    small_manager_t small_manager;
    large_manager_t large_manager;
    uint8_t tiny_set;
    uint8_t small_set;
    uint8_t large_set;
    size_t size;
} alloc_manager_t;

extern alloc_manager_t *alloc_manager;

short AllocManager_init(alloc_manager_e manager);
void AllocManager_uninit(alloc_manager_e manager);
short is_mmap_safe(size_t size);

#ifdef FT_BONUS
#include <pthread.h>
extern pthread_mutex_t alloc_mutex;
#endif /* FT_BONUS */

#endif /* IG_ALLOC_MANAGER */