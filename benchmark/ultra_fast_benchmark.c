/*
** RistrettoDB V2 write-throughput benchmark.
**
** Measures append throughput for the Table V2 engine and prints rows/sec and
** ns/row, with a malloc baseline for context.
**
** Reproducible configuration (the committed, named config):
**   compiler flags : -O3 -std=c11 (portable baseline, NO -march=native)
**   rows           : 1,000,000 (BENCHMARK_ROWS)
**   schema         : (id INTEGER, data TEXT(16))
**   machine class  : modern arm64/x86-64 laptop/server, warm page cache
**   build + run    : make -C benchmark run-ultra-fast
**
** Throughput is flag- and machine-sensitive; always report the config above
** alongside any number.
**
** An OPTIONAL SQLite comparison is available but OFF by default (there is no
** SQL engine in RistrettoDB to compare against; this just contrasts with an
** external library). Enable it with:
**   cc -O3 -std=c11 -DRISTRETTO_BENCH_SQLITE ultra_fast_benchmark.c \
**      ../src/table_v2.c -lsqlite3 -o bench
** CI never builds with it, so sqlite3 is not a build dependency.
*/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../include/table_v2.h"

#ifdef RISTRETTO_BENCH_SQLITE
#include <sqlite3.h>
#endif

#define BENCHMARK_ROWS 1000000

static double get_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// RistrettoDB V2 benchmark
static double benchmark_ristretto_writes(void) {
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

// Memory allocation baseline (lower bound for per-row overhead)
static double benchmark_memory_baseline(void) {
    double start = get_time_seconds();
    for (int i = 0; i < BENCHMARK_ROWS; i++) {
        void *ptr = malloc(16);
        if (ptr) free(ptr);
    }
    return get_time_seconds() - start;
}

#ifdef RISTRETTO_BENCH_SQLITE
static double benchmark_sqlite_writes(void) {
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
#endif

int main(void) {
    printf("RistrettoDB V2 Write-Throughput Benchmark\n");
    printf("=========================================\n");
    printf("Rows: %d  |  schema: (id INTEGER, data TEXT(16))\n\n", BENCHMARK_ROWS);

    printf("Running RistrettoDB V2 benchmark...\n");
    double ristretto_time = benchmark_ristretto_writes();

    printf("Running memory allocation baseline...\n");
    double baseline_time = benchmark_memory_baseline();

    if (ristretto_time < 0) {
        printf("Benchmark failed!\n");
        return 1;
    }

    double ristretto_rows_per_sec = BENCHMARK_ROWS / ristretto_time;
    double ristretto_ns_per_row   = (ristretto_time * 1e9) / BENCHMARK_ROWS;
    double baseline_rows_per_sec  = BENCHMARK_ROWS / baseline_time;
    double baseline_ns_per_row    = (baseline_time * 1e9) / BENCHMARK_ROWS;

    printf("\nResults:\n========\n\n");

    printf("RistrettoDB V2 Performance:\n");
    printf("  Time:        %.3f seconds\n", ristretto_time);
    printf("  Throughput:  %.0f rows/sec\n", ristretto_rows_per_sec);
    printf("  Latency:     %.0f ns/row\n\n", ristretto_ns_per_row);

    printf("Memory Allocation Baseline:\n");
    printf("  Time:        %.3f seconds\n", baseline_time);
    printf("  Throughput:  %.0f ops/sec\n", baseline_rows_per_sec);
    printf("  Latency:     %.0f ns/op\n\n", baseline_ns_per_row);

    printf("  Overhead vs malloc:  %.2fx\n", ristretto_time / baseline_time);

#ifdef RISTRETTO_BENCH_SQLITE
    printf("\nRunning OPTIONAL SQLite (in-memory) contrast...\n");
    double sqlite_time = benchmark_sqlite_writes();
    printf("SQLite (:memory:) Performance:\n");
    printf("  Time:        %.3f seconds\n", sqlite_time);
    printf("  Throughput:  %.0f rows/sec\n", BENCHMARK_ROWS / sqlite_time);
    printf("  Latency:     %.0f ns/row\n", (sqlite_time * 1e9) / BENCHMARK_ROWS);
    printf("  (Contrast only: RistrettoDB has no SQL engine; this is SQLite's\n");
    printf("   in-memory INSERT path, a different workload.)\n");
#endif

    printf("\nTarget Achievement:\n");
    printf("  < 1000ns per row: %s\n", ristretto_ns_per_row < 1000 ? "ACHIEVED" : "Not yet");
    printf("  > 1M rows/sec:    %s\n", ristretto_rows_per_sec > 1000000 ? "ACHIEVED" : "Not yet");

    system("rm -rf data/");
    return 0;
}
