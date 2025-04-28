#include "../../ZoneAllocatorTiny/inc_pub/zone_allocator_tiny.h"
#include <stddef.h>

void ft_free(void* ptr)
{
    short ret;
    ret = ZoneAllocatorTiny_free(ptr);
    /*if (ret != 0)
    {
        ret = ZoneAllocatorSmall_free(ptr);
    }
    if (ret != 0)
    {
        ZoneAllocatorBig_free(ptr);
    }*/
}