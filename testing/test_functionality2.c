/* malloc_test.c */
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <dlfcn.h>
#include "../src/main/inc_pub/malloc.h"


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
    show_alloc_mem();
    test_basic_allocation();
    test_allocation_limits_tiny();
    test_allocation_limits_small();
    test_allocation_limits_large();
    show_alloc_mem();
    test_realloc_basic();
    show_alloc_mem();
    test_realloc_class_change();
    show_alloc_mem();
    test_realloc_limits();
    show_alloc_mem();
    test_free_and_reuse();
    test_edge_cases();

    printf("=== ALL TESTS COMPLETED ===\n");
    show_alloc_mem();
    return 0;
}

/* === TEST IMPLEMENTATIONS (copied & adjusted from first version) === */

void test_basic_allocation(void) {
    printf("=== test1 ===\n");
    void *p1 = malloc(10);
    if (p1 == NULL) {
        TEST_FAIL("Failed to allocate 10 bytes");
        return;
    }
    memset(p1, 0xAA, 10);

    void *p2 = malloc(100);
    if (p2 == NULL) {
        TEST_FAIL("Failed to allocate 100 bytes");
        free(p1);
        return;
    }

    void *p3 = malloc(10000);
    if (p3 == NULL) {
        TEST_FAIL("Failed to allocate 10000 bytes");
        free(p1); free(p2);
        return;
    }

    free(p1);
    free(p2);
    free(p3);
    TEST_PASS();
}

void test_allocation_limits_tiny(void) {
    void *ptrs[MAX_PER_CLASS + 1];
    int i;

    for (i = 0; i < MAX_PER_CLASS; i++) {
        ptrs[i] = malloc(TINY_MAX_SIZE);
        if (ptrs[i] == NULL) {
            TEST_FAIL("Failed to allocate tiny before limit");
            goto cleanup_tiny;
        }
    }

    ptrs[i] = malloc(TINY_MAX_SIZE);
    if (ptrs[i] != NULL) {
        TEST_FAIL("Allocated beyond tiny limit");
        free(ptrs[i]);
        goto cleanup_tiny;
    }

    TEST_PASS();

cleanup_tiny:
    for (int j = 0; j < i; j++) {
        free(ptrs[j]);
    }
}

void test_allocation_limits_small(void) {
    void *ptrs[MAX_PER_CLASS + 1];
    int i;
    for (i = 0; i < MAX_PER_CLASS-1; i++) {
        ptrs[i] = malloc(TINY_MAX_SIZE + 1);
        if (ptrs[i] == NULL) {
            TEST_FAIL("Failed to allocate small before limit");
            goto cleanup_small;
        }
    }

    ptrs[i] = malloc(TINY_MAX_SIZE + 1);
    if (ptrs[i] != NULL) {
        TEST_FAIL("Allocated beyond small limit");
        free(ptrs[i]);
        goto cleanup_small;
    }
    TEST_PASS();

cleanup_small:
    for (int j = 0; j < i; j++) {
        free(ptrs[j]);
    }
}

void test_allocation_limits_large(void) {
    void *ptrs[MAX_PER_CLASS + 1];
    int i;

    for (i = 0; i < MAX_PER_CLASS; i++) {
        ptrs[i] = malloc(SMALL_MAX_SIZE + 1);
        if (ptrs[i] == NULL) {
            TEST_FAIL("Failed to allocate large before limit");
            goto cleanup_large;
        }
    }

    ptrs[i] = malloc(SMALL_MAX_SIZE + 1);
    if (ptrs[i] != NULL) {
        TEST_FAIL("Allocated beyond large limit");
        free(ptrs[i]);
        goto cleanup_large;
    }

    TEST_PASS();

cleanup_large:
    for (int j = 0; j < i; j++) {
        free(ptrs[j]);
    }
}

void test_realloc_basic(void) {
    void *p = malloc(10);
    if (p == NULL) {
        TEST_FAIL("Initial malloc failed");
        return;
    }
    memset(p, 0xBB, 10);

    void *new_p = realloc(p, 20);
    if (new_p == NULL) {
        TEST_FAIL("Realloc to larger failed");
        free(p);
        return;
    }
    if (memcmp(new_p, "\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB", 10) != 0) {
        TEST_FAIL("Data not preserved in realloc");
        free(new_p);
        return;
    }

    new_p = realloc(new_p, 5);
    if (new_p == NULL) {
        TEST_FAIL("Realloc to smaller failed");
        return;
    }

    free(new_p);
    TEST_PASS();
}

void test_realloc_class_change(void) {
    void *p = malloc(10);
    if (p == NULL) {
        TEST_FAIL("Initial tiny malloc failed");
        return;
    }

    void *new_p = realloc(p, 100);
    if (new_p == NULL) {
        TEST_FAIL("Realloc tiny to small failed");
        free(p);
        return;
    }

    new_p = realloc(new_p, 10000);
    if (new_p == NULL) {
        TEST_FAIL("Realloc small to large failed");
        free(new_p);
        return;
    }

    new_p = realloc(new_p, 10);
    if (new_p == NULL) {
        TEST_FAIL("Realloc large to tiny failed");
        free(new_p);
        return;
    }

    free(new_p);
    TEST_PASS();
}

void test_realloc_limits(void) {
    void *ptrs[MAX_PER_CLASS];
    int i;

    for (i = 0; i < MAX_PER_CLASS; i++) {
        ptrs[i] = malloc(10);
        if (ptrs[i] == NULL) {
            TEST_FAIL("Failed to fill tiny for ft_realloc test");
            goto cleanup_realloc_limits;
        }
    }

    void *realloc_p = realloc(ptrs[0], 100);
    if (realloc_p == NULL) {
        TEST_FAIL("Realloc tiny to small failed when tiny full");
        goto cleanup_realloc_limits;
    }
    free(realloc_p);

    void *small_ptrs[MAX_PER_CLASS];
    for (i = 0; i < MAX_PER_CLASS-1; i++) {
        small_ptrs[i] = malloc(100);
        if (small_ptrs[i] == NULL) {
            TEST_FAIL("Failed to fill small for ft_realloc test");
            goto cleanup_realloc_limits_small;
        }
    }

    realloc_p = realloc(ptrs[1], 100);
    if (realloc_p != NULL) {
        TEST_FAIL("Realloc tiny to full small succeeded unexpectedly");
        free(realloc_p);
        goto cleanup_realloc_limits_small;
    };
    TEST_PASS();

cleanup_realloc_limits_small:
    for (i = 0; i < MAX_PER_CLASS; i++) {
        free(small_ptrs[i]);
    }
cleanup_realloc_limits:
    for (i = 1; i < MAX_PER_CLASS; i++) {
        free(ptrs[i]);
    }
}

void test_free_and_reuse(void) {
    void *p1 = malloc(1000);
    if (p1 == NULL) {
        TEST_FAIL("Initial alloc failed");
        return;
    }

    free(p1);

    void *p2 = malloc(100000);
    if (p2 == NULL) {
        TEST_FAIL("Alloc after free failed");
        return;
    }
    void *ptrs[MAX_PER_CLASS - 1];
    size_t j = 10;
    for (int i = 0; i < MAX_PER_CLASS - 1; i++) {
        ptrs[i] = malloc(j);
        if (ptrs[i] == NULL) {
            TEST_FAIL("Failed to fill for reuse test");
            free(p2);
        }
        j += 10;
    }

    free(p2);

    void *p3 = malloc(100);
    if (p3 == NULL) {
        TEST_FAIL("Alloc after free at limit failed");
        goto cleanup_reuse;
    }

    free(p3);
    TEST_PASS();

cleanup_reuse:
    for (int i = 0; i < MAX_PER_CLASS - 1; i++) {
        free(ptrs[i]);
    }
}

void test_edge_cases(void) {
    void *p0 = malloc(0);
    if (p0 != NULL) {
        free(p0);
    }

    void *p = realloc(NULL, 100);
    if (p == NULL) {
        TEST_FAIL("realloc(NULL, size) failed");
        return;
    }

    void *new_p = realloc(p, 0);
    if (new_p != NULL) {
        TEST_FAIL("realloc to 0 should return NULL");
        free(new_p);
        return;
    }

    free(p);
    TEST_PASS();
}
