#include "../inc_pub/print_utils.h"
#include <unistd.h>

#define HEX_ADDR_BUF_SIZE    sizeof(void *) * 2
#define DECIMAL_BUF_SIZE     50

/**
 * @brief Converts an unsigned integer to a string in the specified base.
 *
 * @details 
 * Writes the string representation of `num` into the user-provided `buffer`
 * using digits 0–9 and A–F for bases up to 16. The result is null-terminated.
 * Does not handle negative numbers.
 *
 * @param num    Number to convert
 * @param base   Conversion base (2 to 16 inclusive)
 * @param buffer Destination buffer (must be large enough)
 * @return       Pointer to the resulting null-terminated string (`buffer`),
 *               or NULL if the base is invalid
 */
char *itoa(size_t num, int base, char *buffer)
{
    if (base < 2 || base > 16)
    {
        return NULL; // Invalid base
    }

    char *num_str = buffer;
    int i = 0;
    int j = 0;
    size_t temp = num;

    while (temp != 0)
    {
        int remainder = temp % base;
        if (remainder < 10)
        {
            num_str[i++] = '0' + remainder;
        }
        else
        {
            num_str[i++] = 'A' + (remainder - 10);
        }
        temp /= base;
    }

    num_str[i] = '\0';

    /* Reverse the string in-place */
    for (j = 0; j < i / 2; j++)
    {
        char temp = num_str[j];
        num_str[j] = num_str[i - j - 1];
        num_str[i - j - 1] = temp;
    }

    return num_str;
}

/**
 * @brief Converts an unsigned integer to a fixed-width string with leading zeros.
 *
 * @details 
 * Same as itoa(), but pads the result with leading '0's until exactly `size`
 * digits are written. Useful for printing fixed-width hexadecimal addresses.
 *
 * @param num    Number to convert
 * @param base   Conversion base (2 to 16)
 * @param size   Minimum number of digits (output will be exactly this length)
 * @param buffer Destination buffer (must be at least `size + 1` bytes)
 * @return       Pointer to the resulting string (`buffer`), or NULL on invalid args
 */
char *itoa_size(size_t num, int base, int size, char *buffer)
{
    if (base < 2 || base > 16)
    {
        return NULL;
    }
    if (size <= 0)
    {
        return NULL;
    }

    char *num_str = buffer;
    int i = 0;
    int j = 0;
    size_t temp = num;

    while (temp != 0)
    {
        int remainder = temp % base;
        if (remainder < 10)
        {
            num_str[i++] = '0' + remainder;
        }
        else
        {
            num_str[i++] = 'A' + (remainder - 10);
        }
        temp /= base;
    }

    /* Pad with leading zeros */
    while (i < size)
    {
        num_str[i++] = '0';
    }

    num_str[i] = '\0';

    /* Reverse the string in-place */
    for (j = 0; j < i / 2; j++)
    {
        char temp = num_str[j];
        num_str[j] = num_str[i - j - 1];
        num_str[i - j - 1] = temp;
    }

    return num_str;
}

/**
 * @brief Prints a pointer address in full 64-bit hexadecimal format.
 *
 * Output format: `0xFFFFFFFFFFFFFFFF`
 *
 * @param ptr Pointer whose address to print
 */
void print_address_as_hex(void *ptr)
{
    char num_str[HEX_ADDR_BUF_SIZE];
    unsigned long address = (unsigned long)ptr;

    itoa_size(address, 16, HEX_ADDR_BUF_SIZE, num_str);

    write(1, "0x", 2);
    write(1, num_str, HEX_ADDR_BUF_SIZE);
}

/**
 * @brief Prints n byte dump in format
 *
 * Output format: FF FF FF ...
 *
 * @param ptr Pointer whose address to print
 */
void print_dump(void *ptr, size_t n)
{
    unsigned char *ptr_c = (unsigned char*)ptr;
    size_t num;
    char byte[3] = {0};

    while (n != 0)
    {
        num = (size_t)*ptr_c;
        itoa_size(num, 16, 2, byte);
        write(1, byte, 2);
        write(1, " ", 1);
        n--;
        if (n == 0u)
        {
            break;
        }
        ptr_c++;
    }
}
/**
 * @brief Prints a size value in both decimal and hexadecimal with unit.
 *
 * Output format: `12345 (0x3039) bytes`
 *
 * @param size Size in bytes to display
 */
void print_size(size_t size)
{
    char num_str[DECIMAL_BUF_SIZE];
    size_t cnt;

    /* Decimal part */
    itoa(size, 10, num_str);
    for (cnt = 0; cnt < DECIMAL_BUF_SIZE; cnt++)
    {
        if (num_str[cnt] == '\0')
        {
            break;
        }
    }
    write(1, num_str, cnt);

    /* Hex part */
    write(1, " (0x", 4);
    itoa(size, 16, num_str);
    for (cnt = 0; cnt < DECIMAL_BUF_SIZE; cnt++)
    {
        if (num_str[cnt] == '\0')
        {
            break;
        }
    }
    write(1, num_str, cnt);

    write(1, ") bytes", 7);
}

void print_dump_header(size_t n)
{
    const char line[] = "Address \\ Offset     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f";
    // each number takes exactly 2 characters: space + digit
    // we want columns 0 to n → n+1 columns
    size_t bytes = 16                     // "Address \\ Offset  "
                 + (n + 1) * 3;            // n+1 numbers × " X"

    write(STDOUT_FILENO, line, bytes);
}

/**
 * @brief Copies `len` bytes from `src` to `dest`.
 *
 * Simple byte-by-byte implementation (used internally by the allocator).
 * Safe when `dest` is NULL (does nothing).
 *
 * @param dest Destination memory area
 * @param src  Source memory area
 * @param len  Number of bytes to copy
 */
void ft_memcpy(void *dest, void *src, size_t len)
{
    unsigned char *dest_u = (unsigned char *)dest;
    unsigned char *src_u = (unsigned char *)src;

    if (dest != NULL)
    {
        while (len > 0)
        {
            *dest_u++ = *src_u++;
            len--;
        }
    }
}


