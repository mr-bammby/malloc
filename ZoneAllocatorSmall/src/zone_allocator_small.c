#include "../inc_pub/zone_allocator_small.h"
#include <sys/mman.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>



typedef struct
{
    size_t size; // Size of the block
    size_t used; // Used size of the block
    void *next; // Pointer to the next block
} small_zone_header_t;


#define SMALL_ALLOC_COUNT 100u // Number of allocations
#define SMALL_HEADER_SIZE sizeof(small_zone_header_t) // Size of the header
#define SMALL_ZONE_SIZE ((SMALL_ALLOC_SIZE_MAX + SMALL_HEADER_SIZE) * SMALL_ALLOC_COUNT)  // Total size of the small zone
#define SMALL_ALLOC_ALIGMENT 16u // Alignment of the small allocation


void *small_zone_start = NULL; // Pointer to the small zone
void *small_zone_end = NULL; // Pointer to the small zone
size_t small_zone_mapped_size = 0u;
uint16_t small_alloced_cnt = 0u; // Number of allocated blocks



// Allocates a block of memory of the given size.
void *ZoneAllocatorSmall_alloc(size_t size)
{
    small_zone_header_t *start_header;


    if (size < SMALL_ALLOC_SIZE_MIN || size > SMALL_ALLOC_SIZE_MAX)
    {
        return NULL; // Invalid size
    }
    if (small_zone_start == NULL)
    {
        int page_size = sysconf(_SC_PAGESIZE) ; // Get the page size
        small_zone_mapped_size = SMALL_ZONE_SIZE / page_size; // Calculate the number of aligned blocks
        small_zone_mapped_size = (small_zone_mapped_size * page_size) + ((SMALL_ZONE_SIZE % page_size == 0u) ? (0u) : (page_size)); // Align to page size
        small_zone_start = mmap(NULL, small_zone_mapped_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (small_zone_start == MAP_FAILED)
        {
            return NULL; // Allocation failed
        }
        small_zone_end = (void *)((uint8_t *)small_zone_start + SMALL_ZONE_SIZE); // Set the end pointer
        start_header = (small_zone_header_t *)small_zone_start; // Set the start header
        start_header->size = small_zone_mapped_size; // Set the size of the block
        start_header->used = 0u; // Set the used flag
        start_header->next = NULL; // Set the next pointer
        small_alloced_cnt = 0u;
    }
    small_zone_header_t *current_header = (small_zone_header_t *)small_zone_start; // Set the current header
    size_t aligned_size = (size + SMALL_HEADER_SIZE) / SMALL_ALLOC_ALIGMENT; // Calculate the aligned size
    aligned_size = aligned_size * SMALL_ALLOC_ALIGMENT; // Align the size
    aligned_size += (aligned_size % SMALL_ALLOC_ALIGMENT == 0u) ? (0u) : (SMALL_ALLOC_ALIGMENT); // Align to 16
    while (current_header != NULL)
    {
        if (current_header->used == 0u && current_header->size >= aligned_size)
        {
            current_header->used = size; // Mark the block as used
            current_header->size = aligned_size; // Set the size of the block
            small_alloced_cnt++;
            if (current_header->size > aligned_size)
            {
                small_zone_header_t *next_header = (small_zone_header_t *)((uint8_t *)current_header + aligned_size); // Set the next header
                next_header->size = current_header->size - aligned_size; // Set the size of the next block
                next_header->used = 0u; // Set the used flag
                next_header->next = current_header->next; // Set the next pointer
                current_header->next = next_header; // Set the next pointer of the current block
            }
            break;
        }
        current_header = (small_zone_header_t *)current_header->next; // Move to the next block
    }
    if (current_header == NULL)
    {
        return NULL; // No free block found
    }
    return ((void *)((uint8_t *)current_header + SMALL_HEADER_SIZE)); // Return the pointer to the allocated memory
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
short SmallAllocatorTiny_free(void *ptr)
{
    short ret = 0;
    if (ptr == NULL)
    {
        ret = -1; // Invalid pointer
    }
    else if (ptr < small_zone_start || ptr > small_zone_end)
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
            uint8_t i;
            for (i = 0; i < TINY_ALLOC_COUNT; i++)
            {
                if (tiny_zone_map[i] != 0u)
                {
                    break;
                }
            }
            if (i == TINY_ALLOC_COUNT)
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
