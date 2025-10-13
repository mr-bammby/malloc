#include "../main/inc_pub/malloc.h"
#include "../ZoneAllocatorTiny/inc_pub/zone_allocator_tiny.h"
#include "../ZoneAllocatorSmall/inc_pub/zone_allocator_small.h"
#include "../ZoneAllocatorBig/inc_pub/zone_allocator_big.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

// Benchmark function typedefs
typedef void* (*alloc_func)(size_t);
typedef void (*free_func)(void*);
typedef void* (*realloc_func)(void*, size_t);

// Compile-time flags:
// Use gcc -DENABLE_REALLOC to include realloc-specific tests in functionality
// -DENABLE_SPEED_BENCHMARK to run speed benchmarks
// -DENABLE_MEMORY_BENCHMARK to run memory benchmarks
// Function to get current RSS in bytes
long get_current_rss() {
    FILE* file = fopen("/proc/self/statm", "r");
    if (!file) return -1;
    long rss;
    if (fscanf(file, "%*s %ld", &rss) != 1) {
        fclose(file);
        return -1;
    }
    fclose(file);
    return rss * getpagesize();
}

// Helper to measure time in nanoseconds
long long get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

// Benchmark function typedefs
typedef void* (*alloc_func)(size_t);
typedef void (*free_func)(void*);
typedef void* (*realloc_func)(void*, size_t);

// Functionality tests (with leak-prone scenarios for Valgrind)
void test_functionality(const char* name, alloc_func alloc, free_func dealloc, realloc_func re_alloc) {
    printf("\n=== Functionality Tests for %s ===\n", name);

    // Test 1: Allocate 0 bytes
    void* p0 = alloc(0);
    printf("Alloc(0): %p (should be non-NULL or NULL, depending on impl)\n", p0);
    dealloc(p0);

    // Test 2: Free NULL
    dealloc(NULL);
    printf("Free(NULL): No crash - OK\n");

    // Test 4: Basic alloc/free across zones
    void* tiny = alloc(10);   // Tiny zone
    void* small = alloc(1100); // Small zone
    void* big = alloc(5000);  // Big zone
    if (tiny && small && big) {
        printf("Alloc tiny(10): %p, small(1000): %p, big(5000): %p - OK\n", tiny, small, big);
    }
    dealloc(tiny);
    dealloc(small);
    dealloc(big);

    // Test 6: Intentional leak for Valgrind (unfreed memory)
    void* leak = alloc(64);
    printf("Intentional leak at %p (64 bytes) - check Valgrind output\n", leak);
    // Do not free 'leak' to test Valgrind detection

    // Test 7: Fragmentation and reuse
    void* frags[10];
    for (int i = 0; i < 10; i++) {
        frags[i] = alloc(64);
    }
    for (int i = 0; i < 10; i += 2) {
        dealloc(frags[i]);
    }
    void* new_frag = alloc(64);  // Should reuse freed slot
    printf("Fragmentation reuse: %p - OK\n", new_frag);
    for (int i = 1; i < 10; i += 2) {
        dealloc(frags[i]);
    }
    dealloc(new_frag);

#ifdef ENABLE_REALLOC
    // Test 3: Realloc NULL -> acts like malloc
    void* p1 = re_alloc(NULL, 10);
    if (p1) {
        memset(p1, 0xAA, 10);
        printf("Realloc(NULL, 10): %p - OK\n", p1);
    } else {
        printf("Realloc(NULL, 10): Failed\n");
    }
    dealloc(p1);

    // Test 5: Realloc across zones (tiny -> small -> big)
    void* p2 = alloc(32);
    if (p2) {
        p2 = re_alloc(p2, 1000);  // Tiny -> small
        if (p2) {
            p2 = re_alloc(p2, 5000);  // Small -> big
            if (p2) {
                printf("Realloc(32 -> 1000 -> 5000): %p - OK\n", p2);
            }
        }
        dealloc(p2);
    }

    // Test 8: Realloc to zero size (edge case)
    void* p3 = alloc(128);
    if (p3) {
        p3 = re_alloc(p3, 0);
        printf("Realloc(128 -> 0): %p (should be NULL or valid, depending on impl)\n", p3);
        if (p3) dealloc(p3);
    }
#endif
}

// Speed benchmark
void benchmark_speed(const char* name, alloc_func alloc, free_func dealloc, realloc_func re_alloc) {
    printf("\n=== Speed Benchmark for %s ===\n", name);
    const int iterations = 10000;
    srand(time(NULL));  // Seed random for sizes

    long long start = get_time_ns();
    void* ptrs[100];
    for (int i = 0; i < iterations; i++) {
        size_t size = 1 + rand() % 100000;  // Random size between 1 and 100000
        ptrs[i % 100] = alloc(size);
#ifdef ENABLE_REALLOC
        if (i % 2 == 0) {
            ptrs[i % 100] = re_alloc(ptrs[i % 100], size * 2);
        }
#endif
        dealloc(ptrs[i % 100]);
    }
    long long end = get_time_ns();
    printf("Time for %d operations: %lld ns (%.2f ms)\n", iterations, end - start, (end - start) / 1e6);
}

// Memory usage benchmark
void benchmark_memory(const char* name, alloc_func alloc, free_func dealloc, realloc_func re_alloc) {
    printf("\n=== Memory Usage Benchmark for %s ===\n", name);
    const int num_allocs = 10000;
    srand(time(NULL));  // Seed random for sizes

    long rss_before = get_current_rss();
    if (rss_before == -1) {
        printf("Failed to get RSS\n");
        return;
    }

    void** ptrs = malloc(num_allocs * sizeof(void*));
    if (!ptrs) {
        printf("Failed to alloc ptr array\n");
        return;
    }

    for (int i = 0; i < num_allocs; i++) {
        size_t size = 1 + rand() % 100000;  // Random size between 1 and 100000
        ptrs[i] = alloc(size);
        if (!ptrs[i]) {
            printf("Alloc failed at %d\n", i);
            break;
        }
#ifdef ENABLE_REALLOC
        if (i % 2 == 0) {
            ptrs[i] = re_alloc(ptrs[i], size * 2);
        }
#endif
    }

    long rss_after = get_current_rss();
    if (rss_after == -1) {
        printf("Failed to get RSS\n");
    } else {
        printf("RSS before: %ld bytes\n", rss_before);
        printf("RSS after %d allocs: %ld bytes\n", num_allocs, rss_after);
        printf("Delta: %ld bytes (overhead per alloc ~%.2f bytes)\n", rss_after - rss_before, (double)(rss_after - rss_before) / num_allocs);
    }

    // Clean up
    for (int i = 0; i < num_allocs; i++) {
        if (ptrs[i]) dealloc(ptrs[i]);
    }
    free(ptrs);
}

int main() {
    // Print zone reports after each test for debugging with Valgrind


    printf("\nRunning tests for custom ft_malloc/ft_free/ft_realloc\n");
    test_functionality("Custom", ft_malloc, ft_free, ft_realloc);
    printf("\n=== Custom Allocation Reports After Functionality Test ===\n");
    ZoneAllocatorTiny_report();
    ZoneAllocatorSmall_report();
    ZoneAllocatorBig_report();
#ifdef ENABLE_SPEED_BENCHMARK
    benchmark_speed("Custom", ft_malloc, ft_free, ft_realloc);
#endif
#ifdef ENABLE_MEMORY_BENCHMARK
    benchmark_memory("Custom", ft_malloc, ft_free, ft_realloc);
#endif

    printf("\n=== Final Custom Allocation Reports ===\n");
    ZoneAllocatorTiny_report();
    ZoneAllocatorSmall_report();
    ZoneAllocatorBig_report();

        printf("Running tests for standard malloc/free/realloc\n");
    test_functionality("Standard", malloc, free, realloc);
#ifdef ENABLE_SPEED_BENCHMARK
    benchmark_speed("Standard", malloc, free, realloc);
#endif
#ifdef ENABLE_MEMORY_BENCHMARK
    benchmark_memory("Standard", malloc, free, realloc);
#endif

    return 0;
}