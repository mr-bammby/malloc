/* malloc_test.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/main/inc_pub/malloc.h"
#include <unistd.h>

static void at(void) {
    write(1, "atexit called\n", 14);
}


int main() {

    atexit(at);
    printf("Starting test...\n");
    char* p = malloc(10);
    p = malloc(20);
    p = malloc(30);
    strcpy(p, "Meggie");
    printf("malloc(10) = %p\n", p);
    void* r = malloc(100);
    r = malloc(100);
    r = malloc(100);

    r = malloc(10000);
    r = malloc(10000);
    r = malloc(10000);
    printf("malloc(80000) = %p\n", r);
    show_alloc_mem_ex();
    free(p);
    free(r);
    show_alloc_mem_ex();
    printf("Done.\n");
    exit(0);
}