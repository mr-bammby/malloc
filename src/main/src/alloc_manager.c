#include "alloc_manager.h"
#include <sys/mman.h>
#include <unistd.h>
#include <sys/resource.h>

#ifdef FT_BONUS

/** Global mutex protecting all allocation operations (thread-safety bonus) */
pthread_mutex_t alloc_mutex;


__attribute__((constructor))
void init_alloc_mutex(void) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);  // Key change here
    if (pthread_mutex_init(&alloc_mutex, NULL) != 0) {
        write(STDERR_FILENO, "Pthread_mutex_init failed\n", 27);
    }

    write(STDERR_FILENO, "Pthread_mutex_init good\n", 25);
}

// Optional: Destroy on unload (rarely needed)
__attribute__((destructor))
void destroy_alloc_mutex(void) {
    //pthread_mutex_destroy(&alloc_mutex); 
}
#endif /* FT_BONUS */

/** Global allocation manager instance — allocated once via mmap, never resized */
alloc_manager_t *alloc_manager = NULL;

/**
 * @brief Initializes the global allocation manager and marks a specific zone as active.
 *
 * @details
 * On first use, allocates a single page via mmap to hold the entire manager structure.
 * Subsequent calls only set the corresponding zone flag.
 *
 * @param manager Identifier of the allocation zone being initialized (TINY/SMALL/LARGE)
 * @return 0 on success, -1 on failure (mmap or resource limit violation)
 */
short AllocManager_init(alloc_manager_e manager)
{
    if (alloc_manager == NULL)
    {
        const long page_size = sysconf(_SC_PAGESIZE); // Get the system page size

        /* Early resource limit check before attempting mmap */
        if (is_mmap_safe(page_size) != 0)
        {
            return -1;
        }

        /* Allocate the manager structure anonymously — no file backing */
        alloc_manager = (alloc_manager_t *)mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                                               MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (alloc_manager == MAP_FAILED)
        {
            return -1;
        }

        /* Record the actual mapped size for later unmapping */
        alloc_manager->size = page_size;
        alloc_manager->realloc_hlp.mem = NULL;
    }

    /* Mark the requested allocation zone as active */
    switch (manager)
    {
        case TINY_MANAGER:
            alloc_manager->tiny_set = 1;
            break;
        case SMALL_MANAGER:
            alloc_manager->small_set = 1;
            break;
        case LARGE_MANAGER:
            alloc_manager->large_set = 1;
            break;
        default:
            break;
    }

    return 0;
}

/**
 * @brief Cleans up zone-specific state and potentially frees the entire manager.
 *
 * @details
 * Resets pointers and counters for the specified zone only when its allocation count
 * reaches zero. If all three zones become inactive, the entire manager structure
 * is unmapped and the global pointer is nulled.
 *
 * @param manager Identifier of the zone being cleaned up
 */
void AllocManager_uninit(alloc_manager_e manager)
{
    if (alloc_manager != NULL)
    {
        switch (manager)
        {
            case TINY_MANAGER:
                /* Only reset tiny zone state if no active allocations remain */
                if (alloc_manager->tiny_manager.tiny_alloc_cnt == 0u)
                {
                    alloc_manager->tiny_manager.tiny_zone_end = NULL;
                    alloc_manager->tiny_manager.tiny_zone_start = NULL;
                    alloc_manager->tiny_manager.tiny_zone_mapped_size = 0u;
                    alloc_manager->tiny_manager.tiny_zone_map = NULL;
                    alloc_manager->tiny_set = 0u;
                }
                break;

            case SMALL_MANAGER:
                if (alloc_manager->small_manager.small_alloc_cnt == 0u)
                {
                    alloc_manager->small_manager.small_zone_end = NULL;
                    alloc_manager->small_manager.small_zone_start = NULL;
                    alloc_manager->small_set = 0u;
                }
                break;

            case LARGE_MANAGER:
                if (alloc_manager->large_manager.large_alloc_cnt == 0u)
                {
                    alloc_manager->large_manager.large_zone_end = NULL;
                    alloc_manager->large_manager.large_zone_start = NULL;
                    alloc_manager->large_set = 0u;
                }
                break;

            default:
                break;
        }

        /* Critical section: if no zones are active, release the entire manager */
        if ((alloc_manager->tiny_set == 0u) && 
            (alloc_manager->small_set == 0u) && 
            (alloc_manager->large_set == 0u))
        {
            munmap((void *)alloc_manager, alloc_manager->size);
            alloc_manager = NULL;  // Prevent dangling pointer usage
        }
    }
}

/**
 * @brief Checks whether an mmap of the given size would violate resource limits.
 *
 * @details
 * Queries RLIMIT_AS (virtual memory) and RLIMIT_DATA (heap + mappings) to ensure
 * the requested mapping is permitted by the system.
 *
 * @param size Size of the prospective mmap region
 * @return 0 if safe, 1 if limit exceeded, -1 on getrlimit() failure
 */
short is_mmap_safe(size_t size)
{
    struct rlimit rl_as, rl_data;

    if (getrlimit(RLIMIT_AS, &rl_as) != 0)
    {
        return -1;
    }
    if (getrlimit(RLIMIT_DATA, &rl_data) != 0)
    {
        return -1;
    }

    /* Compare current soft limits — hard limits are not checked */
    if ((rl_as.rlim_cur < size) || (rl_data.rlim_cur < size))
    {
        return 1;  // Mapping would exceed allowed resources
    }

    return 0;
}
