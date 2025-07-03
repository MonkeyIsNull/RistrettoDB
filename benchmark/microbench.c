#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/resource.h>
#include "../include/db.h"

// Microbenchmark focused on specific operations
#define ITERATIONS 100000

typedef struct {
    double user_time;
    double system_time;
    double wall_time;
    long peak_rss;
} Metrics;

static void get_rusage_time(struct rusage *usage) {
    getrusage(RUSAGE_SELF, usage);
}

static double timeval_to_double(struct timeval *tv) {
    return tv->tv_sec + tv->tv_usec / 1000000.0;
}

static Metrics measure_operation(void (*operation)(void *), void *context) {
    Metrics metrics = {0};
    
    struct rusage start_usage, end_usage;
    struct timespec start_time, end_time;
    
    // Force garbage collection if available
    for (int i = 0; i < 5; i++) {
        malloc(1);
    }
    
    get_rusage_time(&start_usage);
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    operation(context);
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    get_rusage_time(&end_usage);
    
    // Calculate metrics
    metrics.user_time = timeval_to_double(&end_usage.ru_utime) - 
                       timeval_to_double(&start_usage.ru_utime);
    metrics.system_time = timeval_to_double(&end_usage.ru_stime) - 
                         timeval_to_double(&start_usage.ru_stime);
    metrics.wall_time = (end_time.tv_sec - start_time.tv_sec) + 
                       (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    metrics.peak_rss = end_usage.ru_maxrss;
    
    return metrics;
}

// Benchmark contexts
typedef struct {
    RistrettoDB *db;
    int count;
} BenchContext;

// Individual operations
static void bench_single_insert(void *ctx) {
    BenchContext *context = (BenchContext *)ctx;
    char sql[256];
    
    for (int i = 0; i < context->count; i++) {
        snprintf(sql, sizeof(sql), 
                 "INSERT INTO test VALUES (%d, 'bench-%d', %f)", 
                 i, i, i * 1.23);
        ristretto_exec(context->db, sql);
    }
}

static void bench_prepared_insert(void *ctx) {
    BenchContext *context = (BenchContext *)ctx;
    
    // Simulate prepared statement by reusing same format
    const char *template = "INSERT INTO test VALUES (%d, '%s', %f)";
    char name[32] = "prepared";
    
    for (int i = 0; i < context->count; i++) {
        char sql[256];
        snprintf(sql, sizeof(sql), template, i, name, i * 1.23);
        ristretto_exec(context->db, sql);
    }
}

static void bench_select_by_id(void *ctx) {
    BenchContext *context = (BenchContext *)ctx;
    char sql[256];
    
    for (int i = 0; i < context->count; i++) {
        int id = rand() % 10000;
        snprintf(sql, sizeof(sql), "SELECT * FROM test WHERE id = %d", id);
        ristretto_query(context->db, sql, NULL, NULL);
    }
}

static void bench_range_select(void *ctx) {
    BenchContext *context = (BenchContext *)ctx;
    char sql[256];
    
    for (int i = 0; i < context->count / 10; i++) {
        int start = rand() % 9000;
        snprintf(sql, sizeof(sql), 
                 "SELECT * FROM test WHERE id >= %d AND id < %d", 
                 start, start + 1000);
        ristretto_query(context->db, sql, NULL, NULL);
    }
}

static void bench_memory_allocation(void *ctx) {
    BenchContext *context = (BenchContext *)ctx;
    
    // Test memory allocation patterns
    for (int i = 0; i < context->count; i++) {
        void *mem = malloc(4096);  // Page-sized allocation
        memset(mem, 0, 4096);
        free(mem);
    }
}

static void print_metrics(const char *name, Metrics *m, int ops) {
    printf("%-25s | ", name);
    printf("Wall: %7.3fs | ", m->wall_time);
    printf("User: %7.3fs | ", m->user_time);
    printf("Sys: %6.3fs | ", m->system_time);
    printf("RSS: %6ld KB | ", m->peak_rss / 1024);
    printf("Ops/sec: %8.0f\n", ops / m->wall_time);
}

int main(int argc, char *argv[]) {
    printf("RistrettoDB Microbenchmarks\n");
    printf("===========================\n");
    printf("Operations per test: %d\n\n", ITERATIONS);
    
    // Create test database
    RistrettoDB *db = ristretto_open("microbench.db");
    ristretto_exec(db, "CREATE TABLE test (id INTEGER, name TEXT, value REAL)");
    
    // Prepare some data for SELECT tests
    printf("Preparing test data...\n");
    BenchContext prep_ctx = {db, 10000};
    bench_single_insert(&prep_ctx);
    
    printf("\nMicrobenchmark Results:\n");
    printf("%-25s | %-11s | %-11s | %-10s | %-12s | %s\n",
           "Operation", "Wall Time", "User Time", "Sys Time", "Peak RSS", "Throughput");
    printf("=========================================================================================\n");
    
    // Run microbenchmarks
    BenchContext ctx = {db, ITERATIONS};
    Metrics m;
    
    // Test 1: Single INSERT operations
    ristretto_exec(db, "CREATE TABLE bench1 (id INTEGER, name TEXT, value REAL)");
    ctx.db = ristretto_open("bench1.db");
    ristretto_exec(ctx.db, "CREATE TABLE bench1 (id INTEGER, name TEXT, value REAL)");
    m = measure_operation(bench_single_insert, &ctx);
    print_metrics("Single INSERT", &m, ctx.count);
    ristretto_close(ctx.db);
    
    // Test 2: Prepared-style INSERT
    ctx.db = ristretto_open("bench2.db");
    ristretto_exec(ctx.db, "CREATE TABLE test (id INTEGER, name TEXT, value REAL)");
    m = measure_operation(bench_prepared_insert, &ctx);
    print_metrics("Prepared INSERT", &m, ctx.count);
    ristretto_close(ctx.db);
    
    // Test 3: Point SELECT
    ctx.db = db;
    ctx.count = 10000;  // Fewer for SELECT
    m = measure_operation(bench_select_by_id, &ctx);
    print_metrics("Point SELECT", &m, ctx.count);
    
    // Test 4: Range SELECT
    ctx.count = 1000;  // Even fewer for range
    m = measure_operation(bench_range_select, &ctx);
    print_metrics("Range SELECT", &m, ctx.count * 10);  // Each does ~1000 rows
    
    // Test 5: Memory allocation baseline
    ctx.count = ITERATIONS;
    m = measure_operation(bench_memory_allocation, &ctx);
    print_metrics("Memory alloc baseline", &m, ctx.count);
    
    // Cleanup
    ristretto_close(db);
    
    printf("\nNote: RSS measurements may include shared libraries and OS caching.\n");
    
    return 0;
}
