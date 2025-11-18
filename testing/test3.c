/* malloc_test.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    printf("Starting test...\n");
    void* p = malloc(100);
    printf("malloc(100) = %p\n", p);
    //show_alloc_mem();
    free(p);
    printf("Done.\n");
    return 0;
}