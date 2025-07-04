#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sqlite3.h>
#include "../include/table_v2.h"

#define BENCHMARK_ROWS 100000

static double get_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// SQLite benchmark
double benchmark_sqlite_writes(void) {
    sqlite3 *db;
    sqlite3_open(":memory:", &db);
    sqlite3_exec(db, "PRAGMA synchronous = OFF", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA journal_mode = OFF", NULL, NULL, NULL);
    
    sqlite3_exec(db, "CREATE TABLE benchmark (id INTEGER, data TEXT)", NULL, NULL, NULL);
    
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "INSERT INTO benchmark VALUES (?, ?)", -1, &stmt, NULL);
    
    double start = get_time_seconds();
    
    for (int i = 0; i < BENCHMARK_ROWS; i++) {
        sqlite3_bind_int(stmt, 1, i);
        sqlite3_bind_text(stmt, 2, "benchmark_data", -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }
    
    double elapsed = get_time_seconds() - start;
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return elapsed;
}

// RistrettoDB V2 benchmark
double benchmark_ristretto_writes(void) {
    // Clean up any existing files
    system("rm -rf data/");
    
    const char *schema = "CREATE TABLE benchmark (id INTEGER, data TEXT(16))";
    Table *table = table_create("benchmark", schema);
    if (!table) {
        printf("Failed to create table\n");
        return -1;
    }
    
    double start = get_time_seconds();
    
    for (int i = 0; i < BENCHMARK_ROWS; i++) {
        Value values[2];
        values[0] = value_integer(i);
        values[1] = value_text("benchmark_data");
        
        if (!table_append_row(table, values)) {
            printf("Failed to insert row %d\n", i);
            value_destroy(&values[1]);
            table_close(table);
            return -1;
        }
        
        value_destroy(&values[1]);
    }
    
    double elapsed = get_time_seconds() - start;
    
    table_close(table);
    
    return elapsed;
}

// Memory allocation baseline
double benchmark_memory_baseline(void) {
    double start = get_time_seconds();
    
    for (int i = 0; i < BENCHMARK_ROWS; i++) {
        void *ptr = malloc(16);
        if (ptr) {
            free(ptr);
        }
    }
    
    double elapsed = get_time_seconds() - start;
    return elapsed;
}

int main(void) {
    printf("Ultra-Fast Write Performance Benchmark\n");
    printf("======================================\n");
    printf("Testing %d row insertions...\n\n", BENCHMARK_ROWS);
    
    // Run benchmarks
    printf("Running SQLite benchmark...\n");
    double sqlite_time = benchmark_sqlite_writes();
    
    printf("Running RistrettoDB V2 benchmark...\n");
    double ristretto_time = benchmark_ristretto_writes();
    
    printf("Running memory allocation baseline...\n");
    double baseline_time = benchmark_memory_baseline();
    
    if (sqlite_time < 0 || ristretto_time < 0) {
        printf("Benchmark failed!\n");
        return 1;
    }
    
    // Calculate metrics
    double sqlite_rows_per_sec = BENCHMARK_ROWS / sqlite_time;
    double ristretto_rows_per_sec = BENCHMARK_ROWS / ristretto_time;
    double baseline_rows_per_sec = BENCHMARK_ROWS / baseline_time;
    
    double sqlite_ns_per_row = (sqlite_time * 1e9) / BENCHMARK_ROWS;
    double ristretto_ns_per_row = (ristretto_time * 1e9) / BENCHMARK_ROWS;
    double baseline_ns_per_row = (baseline_time * 1e9) / BENCHMARK_ROWS;
    
    double speedup = sqlite_time / ristretto_time;
    double baseline_overhead = ristretto_time / baseline_time;
    
    // Display results
    printf("\nResults:\n");
    printf("========\n\n");
    
    printf("SQLite Performance:\n");
    printf("  Time:        %.3f seconds\n", sqlite_time);
    printf("  Throughput:  %.0f rows/sec\n", sqlite_rows_per_sec);
    printf("  Latency:     %.0f ns/row\n\n", sqlite_ns_per_row);
    
    printf("RistrettoDB V2 Performance:\n");
    printf("  Time:        %.3f seconds\n", ristretto_time);
    printf("  Throughput:  %.0f rows/sec\n", ristretto_rows_per_sec);
    printf("  Latency:     %.0f ns/row\n\n", ristretto_ns_per_row);
    
    printf("Memory Allocation Baseline:\n");
    printf("  Time:        %.3f seconds\n", baseline_time);
    printf("  Throughput:  %.0f ops/sec\n", baseline_rows_per_sec);
    printf("  Latency:     %.0f ns/op\n\n", baseline_ns_per_row);
    
    printf("Performance Comparison:\n");
    printf("  Speedup vs SQLite:      %.2fx\n", speedup);
    printf("  Overhead vs malloc:     %.2fx\n", baseline_overhead);
    
    if (ristretto_ns_per_row < 1000) {
        printf("  ULTRA-FAST: Sub-microsecond writes achieved!\n");
    }
    
    if (speedup > 5.0) {
        printf("  BLAZING: >5x faster than SQLite!\n");
    }
    
    printf("\nTarget Achievement:\n");
    printf("  < 100ns per row: %s\n", ristretto_ns_per_row < 100 ? "ACHIEVED" : "Not yet");
    printf("  > 1M rows/sec:   %s\n", ristretto_rows_per_sec > 1000000 ? "ACHIEVED" : "Not yet");
    
    // Cleanup
    system("rm -rf data/");
    
    return 0;
}