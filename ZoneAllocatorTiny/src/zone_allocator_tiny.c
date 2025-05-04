#include "../inc_pub/zone_allocator_tiny.h"
#include <sys/mman.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>

#define TINY_ALLOC_COUNT 100u // Number of allocations
#define TINY_ZONE_SIZE (TINY_ALLOC_SIZE * TINY_ALLOC_COUNT) + TINY_ALLOC_COUNT  // Total size of the tiny zone
#define TINY_ALLOC_ALIGMENT 16u // Alignment of the tiny allocation

uint8_t *tiny_zone_map = NULL; // Pointer to the tiny zone
void *tiny_zone_start = NULL; // Pointer to the tiny zone
void *tiny_zone_end = NULL; // Pointer to the tiny zone
size_t tiny_zone_mapped_size = 0u;
size_t tiny_alloced_cnt;

// Allocates a block of memory of the given size.
void *ZoneAllocatorTiny_alloc(size_t size)
{
    if (tiny_zone_map == NULL)
    {
        int page_size = sysconf(_SC_PAGESIZE) ; // Get the page size
        tiny_zone_mapped_size= TINY_ALLOC_SIZE / page_size; // Calculate the number of aligned blocks
        tiny_zone_mapped_size = (tiny_zone_mapped_size * page_size) + ((TINY_ALLOC_SIZE % page_size == 0u) ? (0u) : (page_size)); // Align to page size
        tiny_zone_map = mmap(NULL, tiny_zone_mapped_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (tiny_zone_map == MAP_FAILED)
        {
            return NULL; // Allocation failed
        }
        uint8_t aligned_count = TINY_ALLOC_COUNT / TINY_ALLOC_ALIGMENT; // Calculate the number of aligned blocks
        aligned_count = (aligned_count * TINY_ALLOC_ALIGMENT) + ((TINY_ALLOC_COUNT % TINY_ALLOC_ALIGMENT == 0u) ? (0u) : (TINY_ALLOC_ALIGMENT)); // Align to 16
        tiny_zone_start = tiny_zone_map + aligned_count; // Set the start pointer
        tiny_zone_end = tiny_zone_start + TINY_ALLOC_SIZE * TINY_ALLOC_COUNT; // Set the end pointer
        tiny_alloced_cnt = 0u;
    }

    for(uint8_t i = 0u; i < TINY_ALLOC_COUNT; i++)
    {
        if ((tiny_zone_map[i]) == 0u)
        {
            tiny_zone_map[i] = size; // Mark the block as used
            tiny_alloced_cnt++;
            return (tiny_zone_start + (i * TINY_ALLOC_SIZE)); // Return the pointer to the allocated memory
        }
    }

    return (NULL);
}

uint8_t ZoneAllocatorTiny_size_get(void *ptr)
{
    uint8_t ret = 0;

    if (ptr == NULL)
    {
        ret = 0; // Invalid pointer
    }
    else if (ptr < tiny_zone_start || ptr > tiny_zone_end)
    {
        ret = 0; // Pointer out of range
    }
    else if (ptr == tiny_zone_map)
    {
        ret =  0; // Pointer is the map of the zone
    }
    else
    {
        uint8_t index = ((uint8_t*)ptr - (uint8_t *)tiny_zone_start) / TINY_ALLOC_SIZE; // Calculate the index of the block
        if (index * TINY_ALLOC_SIZE + (uint8_t *)tiny_zone_start != (uint8_t *)ptr)
        {
            ret = 0; // Pointer not aligned
        }
        else
        {
            ret = tiny_zone_map[index]; // Return the size of the block
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
    else if (ptr < tiny_zone_start || ptr > tiny_zone_end)
    {
        ret = -2; // Pointer out of range
    }
    else
    {
        uint8_t index = ((uint8_t*)ptr - (uint8_t *)tiny_zone_start) / TINY_ALLOC_SIZE; // Calculate the index of the block
        if (index * TINY_ALLOC_SIZE + (uint8_t *)tiny_zone_start != (uint8_t *)ptr)
        {
            ret = -2; // Pointer not aligned
        }
        else  // Free the block
        {
            tiny_zone_map[index] = 0u; // Mark the block as free
            tiny_alloced_cnt--;
            if (tiny_alloced_cnt == 0u)
            {
                munmap((void *)tiny_zone_map, tiny_zone_mapped_size);
                tiny_zone_mapped_size = 0u;
                tiny_zone_map = NULL;
            }
        }
    }
    return (ret);
}

// This function only performs a realocation if the pointer is valid and the size is valid for the tiny zone.
// It checks if the pointer is in the tiny zone.
void *ZoneAllocatorTiny_realloc(void *ptr, size_t size)
{
    void *ret = ptr;
    if (ptr == NULL)
    {
        ret = NULL; // Invalid pointer
    }
    else if (ptr < tiny_zone_start || ptr > tiny_zone_end)
    {
        ret = NULL; // Pointer out of range
    }
    else if ((size == 0) || (size > TINY_ALLOC_SIZE))
    {
        ret =  NULL; // Invalid size
    }
    else if (ptr < tiny_zone_start || ptr > tiny_zone_end)
    {
        ret = NULL; // Pointer out of range
    }
    else if (ptr == tiny_zone_map)
    {
        ret = NULL; // Pointer is the map of the zone
    }
    else
    {
        uint8_t index = ((uint8_t*)ptr - (uint8_t *)tiny_zone_start) / TINY_ALLOC_SIZE; // Calculate the index of the block
        if (index * TINY_ALLOC_SIZE + (uint8_t *)tiny_zone_start != (uint8_t *)ptr)
        {
            ret = NULL; // Pointer not aligned
        }
        else  // Free the block
        {
            tiny_zone_map[index] = size; // Mark the block as free
        }
    }
}

// Converts an integer to a string in the specified base and size.
// It takes a number, a base, and a buffer to store the result.
// It returns the buffer containing the string representation of the number.
char *itoa(uint8_t num, int base, char *buffer)
{
    if (base < 2 || base > 16)
    {
        return NULL; // Invalid base
    }

    char *num_str = buffer; // Use the provided buffer 
    int i = 0;
    int j = 0;
    int temp = num;
    while (temp != 0)
    {
        int remainder = temp % base;
        if (remainder < 10)
        {
            num_str[i++] = '0' + remainder; // Convert to character
        }
        else
        {
            num_str[i++] = 'A' + (remainder - 10); // Convert to character
        }
        temp /= base;
    }
    num_str[i] = '\0'; // Null-terminate the string
    // Reverse the string
    for (j = 0; j < i / 2; j++)
    {
        char temp = num_str[j];
        num_str[j] = num_str[i - j - 1];
        num_str[i - j - 1] = temp;
    }
    return num_str; // Return the string
}


// This function converts an integer to a string in the specified base.
// It takes a number, a base, a size, and a buffer to store the result.
// It returns the buffer containing the string representation of the number.
// The function fills the buffer with leading zeros if the size is greater than the number of digits.
// The function also reverses the string to get the correct order.
char *itoa_size(uint8_t num, int base, int size, char *buffer)
{
    if (base < 2 || base > 16)
    {
        return NULL; // Invalid base
    }
    if (size <= 0)
    {
        return NULL; // Invalid size
    }

    char *num_str = buffer; // Use the provided buffer 
    int i = 0;
    int j = 0;
    int temp = num;
    while (temp != 0)
    {
        int remainder = temp % base;
        if (remainder < 10)
        {
            num_str[i++] = '0' + remainder; // Convert to character
        }
        else
        {
            num_str[i++] = 'A' + (remainder - 10); // Convert to character
        }
        temp /= base;
    }
    while (i < size)
    {
        num_str[i++] = '0'; // Fill with zeros
    }
    num_str[i] = '\0'; // Null-terminate the string
    // Reverse the string
    for (j = 0; j < i / 2; j++)
    {
        char temp = num_str[j];
        num_str[j] = num_str[i - j - 1];
        num_str[i - j - 1] = temp;
    }
    return num_str; // Return the string
}

// This function prints the address of the pointer in hexadecimal format.
// It converts the address to a string and prints it.
void print_address_as_hex(void *ptr)
{
    char num_str[5]; // Buffer to hold the hex string
    unsigned long address = (unsigned long)ptr;
    itoa_size(address, 16, 4, num_str); // Convert address to hex string
    write (1, "0x", 2); // Print the prefix
    write (1, num_str, 4); // Print the address
}

// This function prints the size of the block in bytes.
// It converts the size to a string and prints it.
void print_size(uint8_t size)
{
    char num_str[50]; // Buffer to hold the hex string
    itoa(size, 10, num_str); // Convert size to string
    for (int i = 0; i < 50; i++)
    {
        if (num_str[i] == '\0')
        {
            break; // End of string
        }
        write (1, &num_str[i], 1); // Print the address
    }
    write (1, " bytes", 6); // Print a new line
}


// This function prints the memory map of the tiny zone.
// It prints the start address, end address, and size of each block.
// It also prints the start address of the tiny zone.
void ZoneAllocatorTiny_report(void)
{
    if (tiny_zone_map == NULL)
    {
        return;
    }
    write (1, "TINY : ", 6);
    print_address_as_hex(tiny_zone_map); // Print the start address
    write (1, "\n", 1);
    for (uint8_t i = 0u; i < TINY_ALLOC_COUNT; i++)
    {
        if (tiny_zone_map[i] != 0u)
        {
            print_address_as_hex(tiny_zone_start + (i * TINY_ALLOC_SIZE)); // Print the address of the block
            write (1, " - ", 3);
            print_address_as_hex(tiny_zone_start + (i * TINY_ALLOC_SIZE) + tiny_zone_map[i]); // Print the end address
            write (1, " : ", 3);
            print_size(tiny_zone_map[i]); // Print the size of the block
            write (1, "\n", 1);
        }
    }
}
