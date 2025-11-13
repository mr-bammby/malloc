#include "zone_allocator_tiny.h"
#include "zone_allocator_small.h"
#include "zone_allocator_large.h"
#include "malloc_priv.h"
#include "print_utils.h"
#include "alloc_manager.h"
#include <stddef.h>

void *ft_realloc(void* ptr, size_t size)
{
    short found = 0;
    void * temp_ptr = ptr;

    #ifdef FT_BONUS
    pthread_mutex_lock(&alloc_mutex);
    #endif /* FT_BONUS */
    if (ptr == NULL)
    {
        return(no_block_malloc(size));
    }

    if(alloc_manager == NULL)
    {
        return (NULL);
    }

    (void)ZoneAllocatorTiny_realloc(&temp_ptr, size);
    if (temp_ptr != NULL)
    {
        return temp_ptr;
    }

    temp_ptr = ptr;
    found = (alloc_manager->realloc_hlp.mem != NULL) ? 1 : 0;

    if (found == 0)
    {
        (void)ZoneAllocatorSmall_realloc(&temp_ptr, size);
        if (temp_ptr != NULL)
        {
            return temp_ptr;
        }

        temp_ptr = ptr;
        found = (alloc_manager->realloc_hlp.mem != NULL) ? 1 : 0;
    }
    if (found == 0)
    {
        (void)ZoneAllocatorLarge_realloc(&temp_ptr, size);
        if (temp_ptr != NULL)
        {
            return temp_ptr;
        }

        temp_ptr = ptr;
        found = (alloc_manager->realloc_hlp.mem != NULL) ? 1 : 0;
    }
    if (found != 0)
    {
        temp_ptr = no_block_malloc(size);
        if (temp_ptr != NULL)
        {
            ft_memcpy(temp_ptr, alloc_manager->realloc_hlp.mem, alloc_manager->realloc_hlp.mem_size);
            
            switch (alloc_manager->realloc_hlp.manager)
            {
                case TINY_MANAGER:
                    ZoneAllocatorTiny_free(alloc_manager->realloc_hlp.mem);
                    break;
                case SMALL_MANAGER:
                    ZoneAllocatorSmall_free(alloc_manager->realloc_hlp.mem);
                    break;
                case LARGE_MANAGER:
                    ZoneAllocatorTiny_free(alloc_manager->realloc_hlp.mem);
                    break;
                default:
                    break;
            }
            alloc_manager->realloc_hlp.mem = NULL;
        }
    }
    else
    {
        temp_ptr = NULL;
    }
    
    #ifdef FT_BONUS
    pthread_mutex_unlock(&alloc_mutex);
    #endif /* FT_BONUS */
    return temp_ptr;
}