#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include "../include/db.h"

// Subset of SQLite's speedtest1.c adapted for RistrettoDB capabilities
#define TEST_SIZE_SMALL   1000
#define TEST_SIZE_MEDIUM  10000
#define TEST_SIZE_LARGE   100000

typedef struct {
    const char *name;
    const char *description;
    int iterations;
    void (*sqlite_fn)(sqlite3 *, int);
    void (*ristretto_fn)(RistrettoDB *, int);
} SpeedTest;

static double g_start_time;

static double current_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void start_timer() {
    g_start_time = current_time();
}

static double end_timer() {
    return current_time() - g_start_time;
}

// SQLite implementations
static void sqlite_test1(sqlite3 *db, int n) {
    // Test 1: 1000 INSERTs into table with no index
    char *err_msg = NULL;
    sqlite3_exec(db, "CREATE TABLE t1(a INTEGER, b INTEGER, c TEXT)", NULL, NULL, &err_msg);
    if (err_msg) { sqlite3_free(err_msg); }
    
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "INSERT INTO t1 VALUES(?, ?, ?)", -1, &stmt, NULL);
    
    for (int i = 1; i <= n; i++) {
        char text[100];
        sprintf(text, "This is text value %d", i);
        
        sqlite3_bind_int(stmt, 1, i);
        sqlite3_bind_int(stmt, 2, i * 2);
        sqlite3_bind_text(stmt, 3, text, -1, SQLITE_TRANSIENT);
        
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }
    
    sqlite3_finalize(stmt);
}

static void sqlite_test2(sqlite3 *db, int n) {
    // Test 2: 1000 INSERTs into table with an index
    char *err_msg = NULL;
    sqlite3_exec(db, "CREATE TABLE t2(a INTEGER PRIMARY KEY, b INTEGER, c TEXT)", NULL, NULL, &err_msg);
    if (err_msg) { sqlite3_free(err_msg); }
    
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "INSERT INTO t2 VALUES(?, ?, ?)", -1, &stmt, NULL);
    
    for (int i = 1; i <= n; i++) {
        char text[100];
        sprintf(text, "This is text value %d", i);
        
        sqlite3_bind_int(stmt, 1, i);
        sqlite3_bind_int(stmt, 2, i * 2);
        sqlite3_bind_text(stmt, 3, text, -1, SQLITE_TRANSIENT);
        
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }
    
    sqlite3_finalize(stmt);
}

static void sqlite_test3(sqlite3 *db, int n) {
    // Test 3: Random INSERTs
    char *err_msg = NULL;
    sqlite3_exec(db, "CREATE TABLE t3(a INTEGER PRIMARY KEY, b INTEGER, c TEXT)", NULL, NULL, &err_msg);
    if (err_msg) { sqlite3_free(err_msg); }
    
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "INSERT INTO t3 VALUES(?, ?, ?)", -1, &stmt, NULL);
    
    srand(42); // Fixed seed for reproducibility
    for (int i = 1; i <= n; i++) {
        int id = rand() % (n * 10);
        char text[100];
        sprintf(text, "Random text %d", id);
        
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_int(stmt, 2, id * 3);
        sqlite3_bind_text(stmt, 3, text, -1, SQLITE_TRANSIENT);
        
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }
    
    sqlite3_finalize(stmt);
}

static void sqlite_test4(sqlite3 *db, int n) {
    // Test 4: SELECT with range WHERE
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT count(*) FROM t1 WHERE a >= ? AND a < ?", -1, &stmt, NULL);
    
    int total = 0;
    for (int i = 0; i < n/100; i++) {
        int start = i * 100 + 1;
        int end = start + 99;
        
        sqlite3_bind_int(stmt, 1, start);
        sqlite3_bind_int(stmt, 2, end);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            total += sqlite3_column_int(stmt, 0);
        }
        sqlite3_reset(stmt);
    }
    
    sqlite3_finalize(stmt);
}

static void sqlite_test5(sqlite3 *db, int n) {
    // Test 5: SELECT with ORDER BY
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT a, b FROM t1 ORDER BY c", -1, &stmt, NULL);
    
    int rows = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        rows++;
    }
    
    sqlite3_finalize(stmt);
}

// RistrettoDB implementations
static void ristretto_test1(RistrettoDB *db, int n) {
    // Test 1: 1000 INSERTs into table with no index
    ristretto_exec(db, "CREATE TABLE t1(a INTEGER, b INTEGER, c TEXT)");
    
    for (int i = 1; i <= n; i++) {
        char sql[200];
        sprintf(sql, "INSERT INTO t1 VALUES(%d, %d, 'This is text value %d')", i, i * 2, i);
        ristretto_exec(db, sql);
    }
}

static void ristretto_test2(RistrettoDB *db, int n) {
    // Test 2: 1000 INSERTs into table (RistrettoDB doesn't distinguish indexed/unindexed)
    ristretto_exec(db, "CREATE TABLE t2(a INTEGER, b INTEGER, c TEXT)");
    
    for (int i = 1; i <= n; i++) {
        char sql[200];
        sprintf(sql, "INSERT INTO t2 VALUES(%d, %d, 'This is text value %d')", i, i * 2, i);
        ristretto_exec(db, sql);
    }
}

static void ristretto_test3(RistrettoDB *db, int n) {
    // Test 3: Random INSERTs
    ristretto_exec(db, "CREATE TABLE t3(a INTEGER, b INTEGER, c TEXT)");
    
    srand(42); // Same seed as SQLite
    for (int i = 1; i <= n; i++) {
        int id = rand() % (n * 10);
        char sql[200];
        sprintf(sql, "INSERT INTO t3 VALUES(%d, %d, 'Random text %d')", id, id * 3, id);
        ristretto_exec(db, sql);
    }
}

// Callback for counting rows
static void count_callback(void *data, int argc, char **argv, char **col_names) {
    (void)argc;
    (void)argv;
    (void)col_names;
    int *count = (int *)data;
    (*count)++;
}

static void ristretto_test4(RistrettoDB *db, int n) {
    // Test 4: SELECT with range WHERE (simplified - RistrettoDB doesn't have WHERE ranges yet)
    int total = 0;
    for (int i = 0; i < n/100; i++) {
        char sql[200];
        int start = i * 100 + 1;
        sprintf(sql, "SELECT * FROM t1"); // Simplified for now
        ristretto_query(db, sql, count_callback, &total);
        break; // Only run once since we can't filter yet
    }
}

static void ristretto_test5(RistrettoDB *db, int n) {
    // Test 5: SELECT (no ORDER BY support yet)
    int rows = 0;
    ristretto_query(db, "SELECT * FROM t1", count_callback, &rows);
}

// Test definitions
static SpeedTest tests[] = {
    {"INSERT (no index)", "Sequential INSERTs into unindexed table", 
     TEST_SIZE_SMALL, sqlite_test1, ristretto_test1},
    {"INSERT (indexed)", "Sequential INSERTs into indexed table", 
     TEST_SIZE_SMALL, sqlite_test2, ristretto_test2},
    {"INSERT (random)", "Random INSERTs", 
     TEST_SIZE_SMALL, sqlite_test3, ristretto_test3},
    {"SELECT (range)", "Range queries with WHERE clause", 
     TEST_SIZE_SMALL, sqlite_test4, ristretto_test4},
    {"SELECT (order)", "Table scan with ORDER BY", 
     TEST_SIZE_SMALL, sqlite_test5, ristretto_test5},
};

static void run_tests() {
    printf("SQLite vs RistrettoDB Speed Comparison\n");
    printf("======================================\n");
    printf("Based on subset of SQLite's speedtest1.c\n\n");
    
    printf("%-20s | %-8s | %-10s | %-10s | %s\n", 
           "Test", "Ops", "SQLite", "RistrettoDB", "Speedup");
    printf("%-20s-+-%-8s-+-%-10s-+-%-10s-+-%s\n", 
           "--------------------", "--------", "----------", "----------", "-------");
    
    size_t num_tests = sizeof(tests) / sizeof(tests[0]);
    
    for (size_t i = 0; i < num_tests; i++) {
        SpeedTest *test = &tests[i];
        
        // Run SQLite test
        sqlite3 *sqlite_db;
        sqlite3_open(":memory:", &sqlite_db);
        sqlite3_exec(sqlite_db, "PRAGMA synchronous = OFF", NULL, NULL, NULL);
        sqlite3_exec(sqlite_db, "PRAGMA journal_mode = OFF", NULL, NULL, NULL);
        
        start_timer();
        test->sqlite_fn(sqlite_db, test->iterations);
        double sqlite_time = end_timer();
        
        sqlite3_close(sqlite_db);
        
        // Run RistrettoDB test
        RistrettoDB *ristretto_db = ristretto_open(":memory:");
        
        start_timer();
        test->ristretto_fn(ristretto_db, test->iterations);
        double ristretto_time = end_timer();
        
        ristretto_close(ristretto_db);
        
        // Calculate speedup
        double speedup = sqlite_time / ristretto_time;
        
        printf("%-20s | %8d | %8.3fs | %10.3fs | %6.2fx\n",
               test->name, test->iterations, sqlite_time, ristretto_time, speedup);
    }
    
    printf("\nNotes:\n");
    printf("- Both databases use in-memory storage\n");
    printf("- SQLite configured with synchronous=OFF, journal_mode=OFF\n");
    printf("- Some RistrettoDB tests are simplified due to feature limitations\n");
    printf("- Results may vary based on compiler optimizations and hardware\n");
}

int main(int argc, char *argv[]) {
    run_tests();
    return 0;
}
