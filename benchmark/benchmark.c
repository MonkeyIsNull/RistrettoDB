#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include "../include/db.h"

#define BENCHMARK_ROWS 10000
#define WARMUP_ROWS 100

typedef struct {
    const char *name;
    void (*run_sqlite)(sqlite3 *db, int count);
    void (*run_ristretto)(RistrettoDB *db, int count);
    int count;
} Benchmark;

typedef struct {
    double sqlite_time;
    double ristretto_time;
    double speedup;
} BenchmarkResult;

// Utility functions
static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static void print_result(const char *name, BenchmarkResult *result) {
    printf("%-30s | SQLite: %8.2f ms | Ristretto: %8.2f ms | Speedup: %.2fx\n",
           name, result->sqlite_time, result->ristretto_time, result->speedup);
}

// SQLite benchmarks
static void sqlite_create_table(sqlite3 *db) {
    const char *sql = "CREATE TABLE IF NOT EXISTS bench ("
                      "id INTEGER PRIMARY KEY, "
                      "name TEXT, "
                      "value REAL)";
    char *err_msg = NULL;
    sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (err_msg) {
        fprintf(stderr, "SQLite error: %s\n", err_msg);
        sqlite3_free(err_msg);
    }
}

static void sqlite_sequential_insert(sqlite3 *db, int count) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO bench VALUES (?, ?, ?)";
    
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    for (int i = 0; i < count; i++) {
        sqlite3_bind_int(stmt, 1, i);
        sqlite3_bind_text(stmt, 2, "test", -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 3, i * 1.5);
        
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }
    
    sqlite3_finalize(stmt);
}

static void sqlite_random_insert(sqlite3 *db, int count) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO bench VALUES (?, ?, ?)";
    
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    for (int i = 0; i < count; i++) {
        int id = rand() % (count * 10);
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_text(stmt, 2, "random", -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 3, id * 2.5);
        
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }
    
    sqlite3_finalize(stmt);
}

static void sqlite_select_all(sqlite3 *db, int count) {
    (void)count;
    sqlite3_stmt *stmt;
    const char *sql = "SELECT * FROM bench";
    
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    int rows = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        rows++;
    }
    (void)rows; // Suppress unused warning
    
    sqlite3_finalize(stmt);
}

static void sqlite_select_where(sqlite3 *db, int count) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT * FROM bench WHERE id < ?";
    
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, count / 2);
    
    int rows = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        rows++;
    }
    (void)rows; // Suppress unused warning
    
    sqlite3_finalize(stmt);
}

// RistrettoDB benchmarks
static void ristretto_create_table(RistrettoDB *db) {
    const char *sql = "CREATE TABLE bench ("
                      "id INTEGER, "
                      "name TEXT, "
                      "value REAL)";
    ristretto_exec(db, sql);
}

static void ristretto_sequential_insert(RistrettoDB *db, int count) {
    char sql[256];
    
    for (int i = 0; i < count; i++) {
        snprintf(sql, sizeof(sql), 
                 "INSERT INTO bench VALUES (%d, 'test', %f)", 
                 i, i * 1.5);
        ristretto_exec(db, sql);
    }
}

static void ristretto_random_insert(RistrettoDB *db, int count) {
    char sql[256];
    
    for (int i = 0; i < count; i++) {
        int id = rand() % (count * 10);
        snprintf(sql, sizeof(sql), 
                 "INSERT INTO bench VALUES (%d, 'random', %f)", 
                 id, id * 2.5);
        ristretto_exec(db, sql);
    }
}

static void select_callback(void *data, int argc, char **argv, char **col_names) {
    (void)argc;
    (void)argv;
    (void)col_names;
    int *count = (int *)data;
    (*count)++;
}

static void ristretto_select_all(RistrettoDB *db, int count) {
    (void)count;
    int rows = 0;
    ristretto_query(db, "SELECT * FROM bench", select_callback, &rows);
    (void)rows; // Suppress unused warning
}

static void ristretto_select_where(RistrettoDB *db, int count) {
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT * FROM bench WHERE id < %d", count / 2);
    
    int rows = 0;
    ristretto_query(db, sql, select_callback, &rows);
}

// Benchmark definitions
static Benchmark benchmarks[] = {
    {"Sequential INSERT", sqlite_sequential_insert, ristretto_sequential_insert, BENCHMARK_ROWS},
    {"Random INSERT", sqlite_random_insert, ristretto_random_insert, BENCHMARK_ROWS / 10},
    {"Full table scan", sqlite_select_all, ristretto_select_all, 1},
    {"SELECT with WHERE", sqlite_select_where, ristretto_select_where, 1},
};

static BenchmarkResult run_benchmark(Benchmark *bench) {
    BenchmarkResult result = {0};
    
    // SQLite benchmark
    sqlite3 *sqlite_db;
    sqlite3_open(":memory:", &sqlite_db);
    sqlite3_exec(sqlite_db, "PRAGMA synchronous = OFF", NULL, NULL, NULL);
    sqlite3_exec(sqlite_db, "PRAGMA journal_mode = OFF", NULL, NULL, NULL);
    
    sqlite_create_table(sqlite_db);
    
    // Warmup for SQLite
    if (strstr(bench->name, "INSERT")) {
        bench->run_sqlite(sqlite_db, WARMUP_ROWS);
    }
    
    double start = get_time_ms();
    bench->run_sqlite(sqlite_db, bench->count);
    result.sqlite_time = get_time_ms() - start;
    
    sqlite3_close(sqlite_db);
    
    // RistrettoDB benchmark
    RistrettoDB *ristretto_db = ristretto_open(":memory:");
    ristretto_create_table(ristretto_db);
    
    // Warmup for RistrettoDB
    if (strstr(bench->name, "INSERT")) {
        bench->run_ristretto(ristretto_db, WARMUP_ROWS);
    }
    
    start = get_time_ms();
    bench->run_ristretto(ristretto_db, bench->count);
    result.ristretto_time = get_time_ms() - start;
    
    ristretto_close(ristretto_db);
    
    // Calculate speedup
    result.speedup = result.sqlite_time / result.ristretto_time;
    
    return result;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    printf("RistrettoDB vs SQLite Benchmark\n");
    printf("================================\n");
    printf("Rows per test: %d\n\n", BENCHMARK_ROWS);
    
    printf("%-30s | %-15s | %-15s | %s\n", 
           "Test", "SQLite Time", "Ristretto Time", "Speedup");
    printf("%-30s-+-%-15s-+-%-15s-+-%s\n", 
           "------------------------------",
           "---------------",
           "---------------",
           "---------");
    
    double total_sqlite = 0;
    double total_ristretto = 0;
    
    size_t num_benchmarks = sizeof(benchmarks) / sizeof(benchmarks[0]);
    
    for (size_t i = 0; i < num_benchmarks; i++) {
        BenchmarkResult result = run_benchmark(&benchmarks[i]);
        print_result(benchmarks[i].name, &result);
        
        total_sqlite += result.sqlite_time;
        total_ristretto += result.ristretto_time;
    }
    
    printf("%-30s-+-%-15s-+-%-15s-+-%s\n", 
           "------------------------------",
           "---------------",
           "---------------",
           "---------");
    
    printf("%-30s | SQLite: %8.2f ms | Ristretto: %8.2f ms | Overall: %.2fx\n",
           "TOTAL", total_sqlite, total_ristretto, total_sqlite / total_ristretto);
    
    return 0;
}
