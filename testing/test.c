#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdatomic.h>  // For atomic counters
#include <sys/time.h>   // For gettimeofday (high-res timing)
#include <fcntl.h>      // For write
#include <stdarg.h>

#define NUM_THREADS 1       // Low to start; increase to 16 for more contention
#define ITERATIONS 500      // Per thread; balanced for limits
#define TINY_MAX 64          // <64 bytes
#define SMALL_MAX 8192       // 64 to <8KB
#define BIN_LIMIT 125        // Your max per bin

// Global atomic counters for bin usage (to enforce limits across threads)
atomic_int g_tiny_count = 0;
atomic_int g_small_count = 0;
atomic_int g_large_count = 0;

// Global start time for elapsed logging
struct timeval g_start_time;

// Mutex for logging to avoid interleaved write (optional but recommended)
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Helper to get elapsed ms since start
long get_elapsed_ms() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec - g_start_time.tv_sec) * 1000 + (now.tv_usec - g_start_time.tv_usec) / 1000;
}

// Safe log helper to avoid printf (which may call malloc)
void safe_log(const char *format, ...) {
    char buf[256];  // Fixed buffer; adjust if needed
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    write(1, buf, strlen(buf));  // Write to stdout (fd 1)
}

// Thread function: Stress with bin-aware allocs/frees/reallocs + logging
void* stress_thread(void* arg) {
    long tid = (long)arg;
    srand(time(NULL) + tid);

    void** tiny_allocs = calloc(BIN_LIMIT / NUM_THREADS, sizeof(void*));
    void** small_allocs = calloc(BIN_LIMIT / NUM_THREADS, sizeof(void*));
    void** large_allocs = calloc(BIN_LIMIT / NUM_THREADS, sizeof(void*));
    int tiny_cnt = 0, small_cnt = 0, large_cnt = 0;

    int failures = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        int bin = rand() % 3;  // 0=tiny, 1=small, 2=large
        size_t size;
        if (bin == 0) size = (rand() % (TINY_MAX - 1)) + 1;
        else if (bin == 1) size = (rand() % (SMALL_MAX - TINY_MAX)) + TINY_MAX;
        else size = SMALL_MAX + (rand() % (SMALL_MAX * 10)) + 1;  // Large: 8KB+

        // Check global limit before alloc
        atomic_int* counter = (bin == 0) ? &g_tiny_count : (bin == 1) ? &g_small_count : &g_large_count;
        if (atomic_load(counter) >= BIN_LIMIT) {
            // Force free one if at limit
            if ((bin == 0 && tiny_cnt > 0) || (bin == 1 && small_cnt > 0) || (bin == 2 && large_cnt > 0)) {
                int idx = rand() % ((bin == 0) ? tiny_cnt : (bin == 1) ? small_cnt : large_cnt);
                void* p = (bin == 0) ? tiny_allocs[idx] : (bin == 1) ? small_allocs[idx] : large_allocs[idx];
                
                // Log free
                pthread_mutex_lock(&log_mutex);
                safe_log("[%ld ms] Thread %ld: free ptr=%p\n", get_elapsed_ms(), tid, p);
                pthread_mutex_unlock(&log_mutex);
                
                free(p);
                atomic_fetch_sub(counter, 1);
                // Shift last to idx
                if (bin == 0) { tiny_allocs[idx] = tiny_allocs[--tiny_cnt]; }
                else if (bin == 1) { small_allocs[idx] = small_allocs[--small_cnt]; }
                else { large_allocs[idx] = large_allocs[--large_cnt]; }
            }
            continue;
        }

        int op = rand() % 10;
        if (op <= 4) {  // Alloc new (higher chance to fill bins)
            // Log alloc attempt
            pthread_mutex_lock(&log_mutex);
            safe_log("[%ld ms] Thread %ld: alloc size=%zu\n", get_elapsed_ms(), tid, size);
            pthread_mutex_unlock(&log_mutex);
            
            void* p = malloc(size);
            if (p) {
                memset(p, 0xAA + tid, size);  // Thread-specific pattern
                
                // Log success
                pthread_mutex_lock(&log_mutex);
                safe_log("[%ld ms] Thread %ld: alloc success ptr=%p size=%zu\n", get_elapsed_ms(), tid, p, size);
                pthread_mutex_unlock(&log_mutex);
                
                atomic_fetch_add(counter, 1);
                if (bin == 0 && tiny_cnt < BIN_LIMIT / NUM_THREADS) tiny_allocs[tiny_cnt++] = p;
                else if (bin == 1 && small_cnt < BIN_LIMIT / NUM_THREADS) small_allocs[small_cnt++] = p;
                else if (bin == 2 && large_cnt < BIN_LIMIT / NUM_THREADS) large_allocs[large_cnt++] = p;
                else {
                    // Immediate free if local slot full
                    pthread_mutex_lock(&log_mutex);
                    safe_log("[%ld ms] Thread %ld: free (immediate) ptr=%p\n", get_elapsed_ms(), tid, p);
                    pthread_mutex_unlock(&log_mutex);
                    free(p);
                }
            } else {
                failures++;
                
                // Log failure
                pthread_mutex_lock(&log_mutex);
                safe_log("[%ld ms] Thread %ld: alloc failed size=%zu\n", get_elapsed_ms(), tid, size);
                pthread_mutex_unlock(&log_mutex);
            }
        } else if (op <= 7) {  // Realloc (if have any in bin)
            int* local_cnt = (bin == 0) ? &tiny_cnt : (bin == 1) ? &small_cnt : &large_cnt;
            void*** allocs = (bin == 0) ? &tiny_allocs : (bin == 1) ? &small_allocs : &large_allocs;
            if (*local_cnt > 0) {
                int idx = rand() % *local_cnt;
                void* p = (*allocs)[idx];
                size_t new_size = (rand() % (size * 2)) + 1;  // Can cross bins, but we don't track that
                
                // Log realloc attempt
                pthread_mutex_lock(&log_mutex);
                safe_log("[%ld ms] Thread %ld: realloc old_ptr=%p new_size=%zu\n", get_elapsed_ms(), tid, p, new_size);
                pthread_mutex_unlock(&log_mutex);
                
                void* new_p = realloc(p, new_size);
                if (new_p) {
                    memset(new_p, 0xBB + tid, new_size);
                    (*allocs)[idx] = new_p;
                    
                    // Log success
                    pthread_mutex_lock(&log_mutex);
                    safe_log("[%ld ms] Thread %ld: realloc success new_ptr=%p new_size=%zu\n", get_elapsed_ms(), tid, new_p, new_size);
                    pthread_mutex_unlock(&log_mutex);
                } else {
                    failures++;
                    
                    // Log failure
                    pthread_mutex_lock(&log_mutex);
                    safe_log("[%ld ms] Thread %ld: realloc failed old_ptr=%p new_size=%zu\n", get_elapsed_ms(), tid, p, new_size);
                    pthread_mutex_unlock(&log_mutex);
                }
            }
        } else {  // Free
            int* local_cnt = (bin == 0) ? &tiny_cnt : (bin == 1) ? &small_cnt : &large_cnt;
            if (*local_cnt > 0) {
                int idx = rand() % *local_cnt;
                void* p = (bin == 0) ? tiny_allocs[idx] : (bin == 1) ? small_allocs[idx] : large_allocs[idx];
                
                // Log free
                pthread_mutex_lock(&log_mutex);
                safe_log("[%ld ms] Thread %ld: free ptr=%p\n", get_elapsed_ms(), tid, p);
                pthread_mutex_unlock(&log_mutex);
                
                free(p);
                atomic_fetch_sub(counter, 1);
                // Shift last
                if (bin == 0) tiny_allocs[idx] = tiny_allocs[--tiny_cnt];
                else if (bin == 1) small_allocs[idx] = small_allocs[--small_cnt];
                else large_allocs[idx] = large_allocs[--large_cnt];
            }
        }

        // Contention booster
        if (rand() % 50 == 0) usleep(rand() % 100);
    }

    // Cleanup with logging
    for (int i = 0; i < tiny_cnt; i++) {
        pthread_mutex_lock(&log_mutex);
        safe_log("[%ld ms] Thread %ld: free (cleanup) ptr=%p\n", get_elapsed_ms(), tid, tiny_allocs[i]);
        pthread_mutex_unlock(&log_mutex);
        free(tiny_allocs[i]);
        atomic_fetch_sub(&g_tiny_count, 1);
    }
    for (int i = 0; i < small_cnt; i++) {
        pthread_mutex_lock(&log_mutex);
        safe_log("[%ld ms] Thread %ld: free (cleanup) ptr=%p\n", get_elapsed_ms(), tid, small_allocs[i]);
        pthread_mutex_unlock(&log_mutex);
        free(small_allocs[i]);
        atomic_fetch_sub(&g_small_count, 1);
    }
    for (int i = 0; i < large_cnt; i++) {
        pthread_mutex_lock(&log_mutex);
        safe_log("[%ld ms] Thread %ld: free (cleanup) ptr=%p\n", get_elapsed_ms(), tid, large_allocs[i]);
        pthread_mutex_unlock(&log_mutex);
        free(large_allocs[i]);
        atomic_fetch_sub(&g_large_count, 1);
    }
    free(tiny_allocs); free(small_allocs); free(large_allocs);

    safe_log("Thread %ld finished: %d alloc failures\n", tid, failures);
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];

    // Set global start time
    gettimeofday(&g_start_time, NULL);

    safe_log("Starting bin-limited multi-threaded stress (%d threads, %d iters)...\n", NUM_THREADS, ITERATIONS);

    for (long i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, stress_thread, (void*)i);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    safe_log("Multi-thread phase done.\n");
    safe_log("Final bin counts: Tiny=%d, Small=%d, Large=%d (should be 0)\n",
           atomic_load(&g_tiny_count), atomic_load(&g_small_count), atomic_load(&g_large_count));

    // Phase 2: Single-threaded bin limit testing (with logging)
    safe_log("\nTesting bin limits directly...\n");

    void* tiny[126], *small[126], *large[126];
    int i;

    // Fill tiny bin to 125 + try 126
    for (i = 0; i < 100; i++) {
        safe_log("[%ld ms] Main: alloc (tiny test) size=32\n", get_elapsed_ms());
        tiny[i] = malloc(32);  // <64
        if (tiny[i]) {
            safe_log("[%ld ms] Main: alloc success ptr=%p size=32\n", get_elapsed_ms(), tiny[i]);
        } else {
            safe_log("[%ld ms] Main: alloc failed size=32\n", get_elapsed_ms());
        }
        if (!tiny[i] && i < 125) { safe_log("Tiny alloc failed early at %d!\n", i); break; }
        if (i == 125 && tiny[i]) { safe_log("Tiny over-alloc succeeded? Should fail.\n"); }
    }
    for (i = 0; i < 100; i++) if (tiny[i]) {
        safe_log("[%ld ms] Main: free (tiny test) ptr=%p\n", get_elapsed_ms(), tiny[i]);
        free(tiny[i]);
    }

    // Fill small
    for (i = 0; i < 100; i++) {
        safe_log("[%ld ms] Main: alloc (small test) size=1024\n", get_elapsed_ms());
        small[i] = malloc(1024);  // 64-8KB
        if (small[i]) {
            safe_log("[%ld ms] Main: alloc success ptr=%p size=1024\n", get_elapsed_ms(), small[i]);
        } else {
            safe_log("[%ld ms] Main: alloc failed size=1024\n", get_elapsed_ms());
        }
        if (!small[i] && i < 125) { safe_log("Small alloc failed early at %d!\n", i); break; }
        if (i == 125 && small[i]) { safe_log("Small over-alloc succeeded? Should fail.\n"); }
    }
    for (i = 0; i < 100; i++) if (small[i]) {
        safe_log("[%ld ms] Main: free (small test) ptr=%p\n", get_elapsed_ms(), small[i]);
        free(small[i]);
    }

    // Fill large
    for (i = 0; i < 100; i++) {
        safe_log("[%ld ms] Main: alloc (large test) size=16384\n", get_elapsed_ms());
        large[i] = malloc(16384);  // >8KB
        if (large[i]) {
            safe_log("[%ld ms] Main: alloc success ptr=%p size=16384\n", get_elapsed_ms(), large[i]);
        } else {
            safe_log("[%ld ms] Main: alloc failed size=16384\n", get_elapsed_ms());
        }
        if (!large[i] && i < 125) { safe_log("Large alloc failed early at %d!\n", i); break; }
        if (i == 125 && large[i]) { safe_log("Large over-alloc succeeded? Should fail.\n"); }
    }
    for (i = 0; i < 100; i++) if (large[i]) {
        safe_log("[%ld ms] Main: free (large test) ptr=%p\n", get_elapsed_ms(), large[i]);
        free(large[i]);
    }

    // Edge cases (with logging)
    safe_log("[%ld ms] Main: alloc (edge) size=0\n", get_elapsed_ms());
    malloc(0);
    safe_log("[%ld ms] Main: realloc (edge) ptr=NULL size=0\n", get_elapsed_ms());
    realloc(NULL, 0);
    safe_log("[%ld ms] Main: free (edge) ptr=NULL\n", get_elapsed_ms());
    free(NULL);
    safe_log("[%ld ms] Main: alloc (edge) size=1\n", get_elapsed_ms());
    void* p = malloc(1);
    if (p) safe_log("[%ld ms] Main: alloc success ptr=%p size=1\n", get_elapsed_ms(), p);
    safe_log("[%ld ms] Main: realloc (edge) ptr=%p new_size=1ULL<<40\n", get_elapsed_ms(), p);
    p = realloc(p, 1ULL << 40);  // Insane size (should fail or handle)
    if (p) safe_log("[%ld ms] Main: realloc success new_ptr=%p new_size=1ULL<<40\n", get_elapsed_ms(), p);
    else safe_log("[%ld ms] Main: realloc failed\n", get_elapsed_ms());
    if (p) {
        safe_log("[%ld ms] Main: free (edge) ptr=%p\n", get_elapsed_ms(), p);
        free(p);
    }

    safe_log("All tests complete. Run under Valgrind for leak/race info.\n");
    return 0;
}