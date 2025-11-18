#ifndef IG_ALLOC_MANAGER
#define IG_ALLOC_MANAGER

#include <stddef.h>
#include <inttypes.h>

#ifdef FT_BONUS
#include <pthread.h>
#endif /* FT_BONUS */

/**
 * @brief Align a value upwards to the nearest multiple of `align`.
 *
 * This macro uses bit manipulation for maximum performance and is fully
 * evaluable at compile-time when both arguments are constants.
 *
 * Example:
 *   ALIGN_UP(37, 8)  → 40
 *   ALIGN_UP(32, 8)  → 32
 *
 * Used heavily for headers and block sizes → must be fast and correct.
 *
 * @note `align` must be a power of 2 (8, 16, 32, 64, etc.).
 *       This is guaranteed in your code (SMALL_ALLOC_ALIGMENT = 8).
 */
#define ALIGN_UP_CONST(value, align) \
    (((value) + (align) - 1) & ~((align) - 1))

/**
 * @brief Align a value upwards — safer runtime version with type preservation.
 *
 * Use this only if you need to silence warnings or support non-constant align.
 * In your case, ALIGN_UP() above is sufficient and preferred.
 */
#define ALIGN_UP(value, align) \
    (((value) + (typeof(value))((align) - 1)) & ~(typeof(value))((align) - 1))

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
    small_block_header_t *first_block; 	// Location of the first block
    size_t cnt; 						// Used for mumap of maps
	size_t size;						// Used for mumap of maps
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
    large_block_header_t *first_block; 	// Location of the first block
	size_t size;						// Used for mumap of maps
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

typedef struct realloc_hlp_s
{
    void* mem;
    size_t mem_size;
    alloc_manager_e manager;
} realloc_hlp_t;


typedef struct alloc_manager_s
{
    tiny_manager_t tiny_manager;
    small_manager_t small_manager;
    large_manager_t large_manager;
    uint8_t tiny_set;
    uint8_t small_set;
    uint8_t large_set;
    size_t size;
    realloc_hlp_t realloc_hlp;
} alloc_manager_t;

extern alloc_manager_t *alloc_manager;

short AllocManager_init(alloc_manager_e manager);
void AllocManager_uninit(alloc_manager_e manager);
short is_mmap_safe(size_t size);

#ifdef FT_BONUS
extern pthread_mutex_t alloc_mutex;
#endif /* FT_BONUS */

#endif /* IG_ALLOC_MANAGER */