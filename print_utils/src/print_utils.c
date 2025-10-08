#include "../inc_pub/print_utils.h"

// Converts an integer to a string in the specified base and size.
// It takes a number, a base, and a buffer to store the result.
// It returns the buffer containing the string representation of the number.
char *itoa(size_t num, int base, char *buffer)
{
    if (base < 2 || base > 16)
    {
        return NULL; // Invalid base
    }

    char *num_str = buffer; // Use the provided buffer 
    int i = 0;
    int j = 0;
    size_t temp = num;
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
char *itoa_size(size_t num, int base, int size, char *buffer)
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
    size_t temp = num;
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
    char num_str[16]; // Buffer to hold the hex string
    unsigned long address = (unsigned long)ptr;
    itoa_size(address, 16, 16, num_str); // Convert address to hex string
    write (1, "0x", 2); // Print the prefix
    write (1, num_str, 16); // Print the address
}

// This function prints the size of the block in bytes.
// It converts the size to a string and prints it.
void print_size(size_t size)
{
    char num_str[50]; // Buffer to hold the num string
    size_t cnt;

    itoa(size, 10, num_str); // Convert size to string
    for (cnt = 0; cnt < 50; cnt++)
    {
        if (num_str[cnt] == '\0')
        {
            break; // End of string
        }
    }
    write (1, num_str, cnt); // Print the address
    write (1, " (0x", 4);
    itoa(size, 16, num_str); // Convert size to string
    for (cnt = 0; cnt < 50; cnt++)
    {
        if (num_str[cnt] == '\0')
        {
            break; // End of string
        }
    }
    write (1, num_str, cnt); // Print the address
    write (1, ") bytes", 7); // Print a new line
}
