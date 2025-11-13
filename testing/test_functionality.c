/* malloc_test.c */
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>

/* ---- Include YOUR header ---- */
#include "../src/main/inc_pub/malloc.h"

/* Runtime pointer for show_alloc_mem */
static void (*show_alloc_mem_ptr)(void) = NULL;

static void init_show_func(void) {
    void* handle = dlopen(NULL, RTLD_LAZY);
    if (handle) {
        void* sym = dlsym(handle, "show_alloc_mem");
        if (sym) show_alloc_mem_ptr = sym;
        dlclose(handle);
    }
}

/* Test macros */
#define TEST_PASS()  printf("PASS: %s\n", __func__)
#define TEST_FAIL(msg) printf("FAIL: %s - %s\n", __func__, (msg))

#define TINY_MAX_SIZE  64
#define SMALL_MAX_SIZE 8192
#define MAX_PER_CLASS  125

/* Test function declarations */
void test_basic_allocation(void);
void test_allocation_limits_tiny(void);
void test_allocation_limits_small(void);
void test_allocation_limits_large(void);
void test_realloc_basic(void);
void test_realloc_class_change(void);
void test_realloc_limits(void);
void test_free_and_reuse(void);
void test_edge_cases(void);
void test_show_alloc_mem(void);

int main(void) {
    printf("=== STARTING MALLOC/REALLOC/FREE TESTS ===\n");
    //init_show_func();  /* Detect show_alloc_mem */

    test_basic_allocation();
    test_allocation_limits_tiny();
    test_allocation_limits_small();
    test_allocation_limits_large();
    test_realloc_basic();
    test_realloc_class_change();
    test_realloc_limits();
    test_free_and_reuse();
    test_edge_cases();

    if (show_alloc_mem_ptr) {
        test_show_alloc_mem();
    } else {
        printf("NOTE: show_alloc_mem() not available (using standard ft_malloc)\n");
    }

    printf("=== ALL TESTS COMPLETED ===\n");
    return 0;
}

/* === TEST IMPLEMENTATIONS (copied & adjusted from first version) === */

void test_basic_allocation(void) {
    printf("=== test1 ===\n");
    void *p1 = ft_malloc(10);
    if (p1 == NULL) {
        TEST_FAIL("Failed to allocate 10 bytes");
        return;
    }
    memset(p1, 0xAA, 10);

    void *p2 = ft_malloc(100);
    if (p2 == NULL) {
        TEST_FAIL("Failed to allocate 100 bytes");
        ft_free(p1);
        return;
    }

    void *p3 = ft_malloc(10000);
    if (p3 == NULL) {
        TEST_FAIL("Failed to allocate 10000 bytes");
        ft_free(p1); ft_free(p2);
        return;
    }

    ft_free(p1);
    ft_free(p2);
    ft_free(p3);
    TEST_PASS();
}

void test_allocation_limits_tiny(void) {
    void *ptrs[MAX_PER_CLASS + 1];
    int i;

    for (i = 0; i < MAX_PER_CLASS; i++) {
        ptrs[i] = ft_malloc(TINY_MAX_SIZE);
        if (ptrs[i] == NULL) {
            TEST_FAIL("Failed to allocate tiny before limit");
            goto cleanup_tiny;
        }
    }

    ptrs[i] = ft_malloc(TINY_MAX_SIZE);
    if (ptrs[i] != NULL) {
        TEST_FAIL("Allocated beyond tiny limit");
        ft_free(ptrs[i]);
        goto cleanup_tiny;
    }

    TEST_PASS();

cleanup_tiny:
    for (int j = 0; j < i; j++) {
        ft_free(ptrs[j]);
    }
}

void test_allocation_limits_small(void) {
    void *ptrs[MAX_PER_CLASS + 1];
    int i;

    for (i = 0; i < MAX_PER_CLASS; i++) {
        ptrs[i] = ft_malloc(TINY_MAX_SIZE + 1);
        if (ptrs[i] == NULL) {
            TEST_FAIL("Failed to allocate small before limit");
            goto cleanup_small;
        }
    }

    ptrs[i] = ft_malloc(TINY_MAX_SIZE + 1);
    if (ptrs[i] != NULL) {
        TEST_FAIL("Allocated beyond small limit");
        ft_free(ptrs[i]);
        goto cleanup_small;
    }

    TEST_PASS();

cleanup_small:
    for (int j = 0; j < i; j++) {
        ft_free(ptrs[j]);
    }
}

void test_allocation_limits_large(void) {
    void *ptrs[MAX_PER_CLASS + 1];
    int i;

    for (i = 0; i < MAX_PER_CLASS; i++) {
        ptrs[i] = ft_malloc(SMALL_MAX_SIZE + 1);
        if (ptrs[i] == NULL) {
            TEST_FAIL("Failed to allocate large before limit");
            goto cleanup_large;
        }
    }

    ptrs[i] = ft_malloc(SMALL_MAX_SIZE + 1);
    if (ptrs[i] != NULL) {
        TEST_FAIL("Allocated beyond large limit");
        ft_free(ptrs[i]);
        goto cleanup_large;
    }

    TEST_PASS();

cleanup_large:
    for (int j = 0; j < i; j++) {
        ft_free(ptrs[j]);
    }
}

void test_realloc_basic(void) {
    void *p = ft_malloc(10);
    if (p == NULL) {
        TEST_FAIL("Initial ft_malloc failed");
        return;
    }
    memset(p, 0xBB, 10);

    void *new_p = ft_realloc(p, 20);
    if (new_p == NULL) {
        TEST_FAIL("Realloc to larger failed");
        ft_free(p);
        return;
    }
    if (memcmp(new_p, "\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB", 10) != 0) {
        TEST_FAIL("Data not preserved in ft_realloc");
        ft_free(new_p);
        return;
    }

    new_p = ft_realloc(new_p, 5);
    if (new_p == NULL) {
        TEST_FAIL("Realloc to smaller failed");
        return;
    }

    ft_free(new_p);
    TEST_PASS();
}

void test_realloc_class_change(void) {
    void *p = ft_malloc(10);
    if (p == NULL) {
        TEST_FAIL("Initial tiny ft_malloc failed");
        return;
    }

    void *new_p = ft_realloc(p, 100);
    if (new_p == NULL) {
        TEST_FAIL("Realloc tiny to small failed");
        ft_free(p);
        return;
    }

    new_p = ft_realloc(new_p, 10000);
    if (new_p == NULL) {
        TEST_FAIL("Realloc small to large failed");
        ft_free(new_p);
        return;
    }

    new_p = ft_realloc(new_p, 10);
    if (new_p == NULL) {
        TEST_FAIL("Realloc large to tiny failed");
        ft_free(new_p);
        return;
    }

    ft_free(new_p);
    TEST_PASS();
}

void test_realloc_limits(void) {
    void *ptrs[MAX_PER_CLASS];
    int i;

    for (i = 0; i < MAX_PER_CLASS; i++) {
        ptrs[i] = ft_malloc(10);
        if (ptrs[i] == NULL) {
            TEST_FAIL("Failed to fill tiny for ft_realloc test");
            goto cleanup_realloc_limits;
        }
    }

    void *realloc_p = ft_realloc(ptrs[0], 100);
    if (realloc_p == NULL) {
        TEST_FAIL("Realloc tiny to small failed when tiny full");
        goto cleanup_realloc_limits;
    }
    ft_free(realloc_p);

    void *small_ptrs[MAX_PER_CLASS];
    for (i = 0; i < MAX_PER_CLASS; i++) {
        small_ptrs[i] = ft_malloc(100);
        if (small_ptrs[i] == NULL) {
            TEST_FAIL("Failed to fill small for ft_realloc test");
            goto cleanup_realloc_limits_small;
        }
    }

    realloc_p = ft_realloc(ptrs[1], 100);
    if (realloc_p != NULL) {
        TEST_FAIL("Realloc tiny to full small succeeded unexpectedly");
        ft_free(realloc_p);
        goto cleanup_realloc_limits_small;
    }

    TEST_PASS();

cleanup_realloc_limits_small:
    for (i = 0; i < MAX_PER_CLASS; i++) {
        ft_free(small_ptrs[i]);
    }
cleanup_realloc_limits:
    for (i = 0; i < MAX_PER_CLASS; i++) {
        ft_free(ptrs[i]);
    }
}

void test_free_and_reuse(void) {
    void *p1 = ft_malloc(1000);
    if (p1 == NULL) {
        TEST_FAIL("Initial alloc failed");
        return;
    }

    ft_free(p1);

    void *p2 = ft_malloc(100000);
    if (p2 == NULL) {
        TEST_FAIL("Alloc after ft_free failed");
        return;
    }
    show_alloc_mem();
    void *ptrs[MAX_PER_CLASS - 1];
    size_t j = 10;
    for (int i = 0; i < MAX_PER_CLASS - 1; i++) {
        ptrs[i] = ft_malloc(j);
        if (ptrs[i] == NULL) {
            TEST_FAIL("Failed to fill for reuse test");
            ft_free(p2);
        }
        j *= 10;
        show_alloc_mem();
    }
    show_alloc_mem();

    ft_free(p2);

    void *p3 = ft_malloc(100);
    if (p3 == NULL) {
        TEST_FAIL("Alloc after ft_free at limit failed");
        goto cleanup_reuse;
    }

    ft_free(p3);
    TEST_PASS();

cleanup_reuse:
    for (int i = 0; i < MAX_PER_CLASS - 1; i++) {
        ft_free(ptrs[i]);
    }
}

void test_edge_cases(void) {
    void *p0 = ft_malloc(0);
    if (p0 != NULL) {
        ft_free(p0);
    }

    void *p = ft_realloc(NULL, 100);
    if (p == NULL) {
        TEST_FAIL("ft_realloc(NULL, size) failed");
        return;
    }

    void *new_p = ft_realloc(p, 0);
    if (new_p != NULL) {
        TEST_FAIL("ft_realloc to 0 should return NULL");
        ft_free(new_p);
        return;
    }

    ft_free(NULL);
    TEST_PASS();
}

/* NEW: test show_alloc_mem */
void test_show_alloc_mem(void) {
    printf("\n=== CALLING show_alloc_mem() ===\n");
    show_alloc_mem_ptr();
    TEST_PASS();
}