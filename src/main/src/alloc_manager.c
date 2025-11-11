#include "alloc_manager.h"
#include <sys/mman.h>
#include <unistd.h>
#include <sys/resource.h>

#ifdef FT_BONUS
pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif /* FT_BONUS */

alloc_manager_t *alloc_manager = NULL;

short AllocManager_init(alloc_manager_e manager)
{
    if (alloc_manager == NULL)
    {
        const long page_size = sysconf(_SC_PAGESIZE) ; // Get the page size

        if (is_mmap_safe(page_size) != 0)
        {
            return -1;
        }
        alloc_manager = (alloc_manager_t *)mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (alloc_manager == MAP_FAILED)
        {
            return -1;
        }
        alloc_manager->size = page_size;
    }
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

void AllocManager_uninit(alloc_manager_e manager)
{
    if (alloc_manager != NULL)
    {
        switch (manager)
        {
            case TINY_MANAGER:
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
        if ((alloc_manager->tiny_set == 0u) && \
            (alloc_manager->small_set == 0u) && \
            (alloc_manager->large_set == 0u))
        {
			munmap((void *)alloc_manager, alloc_manager->size);
            alloc_manager = NULL;
        }
    }
}

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

    if ((rl_as.rlim_cur < size) || (rl_data.rlim_cur < size))
    {
        return 1;
    }

    return 0;
}


