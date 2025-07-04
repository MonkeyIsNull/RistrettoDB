#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "table_v2.h"

// Test framework
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        printf("Running stress test: %s ... ", #name); \
        tests_run++; \
        if (test_##name()) { \
            printf("PASS\n"); \
            tests_passed++; \
        } else { \
            printf("FAIL\n"); \
            tests_failed++; \
        } \
    } while(0)

#define REQUIRE(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", message); \
            return false; \
        } \
    } while(0)

// Cleanup
void cleanup_test_files(void) {
    system("rm -rf data/");
}

// ========================================
// HIGH-VOLUME STRESS TESTS
// ========================================

// Stress test: Large dataset insertion
bool test_large_dataset_insertion(void) {
    cleanup_test_files();
    
    Table* table = table_create("large_dataset", 
        "CREATE TABLE large_dataset (id INTEGER, timestamp INTEGER, value REAL, status INTEGER)");
    REQUIRE(table != NULL, "Failed to create large dataset table");
    
    const int STRESS_ROWS = 1000000; // 1 million rows
    printf("\n    Inserting %d rows for stress test...", STRESS_ROWS);
    
    clock_t start = clock();
    
    for (int i = 0; i < STRESS_ROWS; i++) {
        Value values[4];
        values[0] = value_integer(i);
        values[1] = value_integer(time(NULL) + i);
        values[2] = value_real(i * 1.5);
        values[3] = value_integer(i % 10);
        
        REQUIRE(table_append_row(table, values), "Insert failed during stress test");
        
        // Progress indicator
        if (i % 100000 == 0) {
            printf(".");
            fflush(stdout);
        }
    }
    
    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    double rows_per_sec = STRESS_ROWS / elapsed;
    double ns_per_row = (elapsed * 1e9) / STRESS_ROWS;
    
    printf("\n    Stress test results:");
    printf("\n      Inserted: %d rows", STRESS_ROWS);
    printf("\n      Time: %.3f seconds", elapsed);
    printf("\n      Throughput: %.0f rows/sec", rows_per_sec);
    printf("\n      Latency: %.0f ns/row", ns_per_row);
    printf("\n      File size: %.1f MB", (double)table->mapped_size / (1024 * 1024));
    
    // Validate our performance claims under stress
    REQUIRE(rows_per_sec > 1000000, "Should maintain >1M rows/sec under stress");
    REQUIRE(ns_per_row < 2000, "Latency should stay reasonable under stress");
    REQUIRE(table->header->num_rows == STRESS_ROWS, "Row count mismatch");
    
    table_close(table);
    return true;
}

// Stress test: Memory pressure with text fields
bool test_memory_pressure_text(void) {
    cleanup_test_files();
    
    Table* table = table_create("memory_pressure", 
        "CREATE TABLE memory_pressure (id INTEGER, large_text TEXT(128), medium_text TEXT(64))");
    REQUIRE(table != NULL, "Failed to create memory pressure table");
    
    const int MEMORY_STRESS_ROWS = 100000;
    printf("\n    Testing memory pressure with %d text-heavy rows...", MEMORY_STRESS_ROWS);
    
    // Create test strings of various sizes
    char large_text[129];
    char medium_text[65];
    memset(large_text, 'A', 128);
    large_text[128] = '\0';
    memset(medium_text, 'B', 64);
    medium_text[64] = '\0';
    
    clock_t start = clock();
    
    for (int i = 0; i < MEMORY_STRESS_ROWS; i++) {
        Value values[3];
        values[0] = value_integer(i);
        values[1] = value_text(large_text);
        values[2] = value_text(medium_text);
        
        REQUIRE(table_append_row(table, values), "Insert failed during memory stress");
        
        // Critical: Clean up text allocations
        value_destroy(&values[1]);
        value_destroy(&values[2]);
        
        if (i % 10000 == 0) {
            printf(".");
            fflush(stdout);
        }
    }
    
    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    double rows_per_sec = MEMORY_STRESS_ROWS / elapsed;
    
    printf("\n    Memory pressure results:");
    printf("\n      Rows: %d", MEMORY_STRESS_ROWS);
    printf("\n      Time: %.3f seconds", elapsed);
    printf("\n      Throughput: %.0f rows/sec", rows_per_sec);
    printf("\n      Row size: %u bytes", table->header->row_size);
    printf("\n      Total data: %.1f MB", 
           (double)(MEMORY_STRESS_ROWS * table->header->row_size) / (1024 * 1024));
    
    // Should still maintain reasonable performance with text fields
    REQUIRE(rows_per_sec > 50000, "Should maintain >50K rows/sec with text fields");
    REQUIRE(table->header->num_rows == MEMORY_STRESS_ROWS, "Row count mismatch");
    
    table_close(table);
    return true;
}

// Stress test: File growth behavior
bool test_file_growth_stress(void) {
    cleanup_test_files();
    
    Table* table = table_create("growth_stress", "CREATE TABLE growth_stress (id INTEGER, data INTEGER)");
    REQUIRE(table != NULL, "Failed to create growth stress table");
    
    printf("\n    Testing file growth behavior...");
    
    size_t initial_size = table->mapped_size;
    int growth_count = 0;
    size_t last_size = initial_size;
    
    // Insert data until we see multiple file growths
    for (int i = 0; i < 500000; i++) {
        Value values[2];
        values[0] = value_integer(i);
        values[1] = value_integer(i * 2);
        
        REQUIRE(table_append_row(table, values), "Insert failed during growth stress");
        
        // Check for file growth
        if (table->mapped_size > last_size) {
            growth_count++;
            printf("\n      Growth %d: %zu -> %zu bytes (at row %d)", 
                   growth_count, last_size, table->mapped_size, i);
            last_size = table->mapped_size;
        }
        
        if (growth_count >= 3) break; // Stop after seeing multiple growths
    }
    
    printf("\n    File growth stress results:");
    printf("\n      Initial size: %zu bytes", initial_size);
    printf("\n      Final size: %zu bytes", table->mapped_size);
    printf("\n      Growth events: %d", growth_count);
    printf("\n      Growth factor: %.1fx", (double)table->mapped_size / initial_size);
    
    REQUIRE(growth_count >= 1, "Should have seen at least one file growth");
    REQUIRE(table->mapped_size > initial_size, "File should have grown");
    
    table_close(table);
    return true;
}

// Stress test: Rapid table creation/destruction
bool test_rapid_table_lifecycle(void) {
    cleanup_test_files();
    
    printf("\n    Testing rapid table creation/destruction...");
    
    const int LIFECYCLE_COUNT = 1000;
    clock_t start = clock();
    
    for (int i = 0; i < LIFECYCLE_COUNT; i++) {
        char table_name[64];
        snprintf(table_name, sizeof(table_name), "temp_table_%d", i);
        
        // Create table
        Table* table = table_create(table_name, "CREATE TABLE temp (id INTEGER, value REAL)");
        REQUIRE(table != NULL, "Failed to create temporary table");
        
        // Insert some data
        for (int j = 0; j < 10; j++) {
            Value values[2];
            values[0] = value_integer(j);
            values[1] = value_real(j * 1.5);
            REQUIRE(table_append_row(table, values), "Failed to insert into temp table");
        }
        
        // Close table
        table_close(table);
        
        if (i % 100 == 0) {
            printf(".");
            fflush(stdout);
        }
    }
    
    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    double tables_per_sec = LIFECYCLE_COUNT / elapsed;
    
    printf("\n    Rapid lifecycle results:");
    printf("\n      Tables created/destroyed: %d", LIFECYCLE_COUNT);
    printf("\n      Time: %.3f seconds", elapsed);
    printf("\n      Rate: %.0f tables/sec", tables_per_sec);
    
    REQUIRE(tables_per_sec > 100, "Should handle >100 table creations/sec");
    
    return true;
}

// Stress test: Concurrent-style operations (simulate threading load)
bool test_simulated_concurrent_load(void) {
    cleanup_test_files();
    
    // Create multiple tables to simulate concurrent workload
    Table* tables[5];
    const char* table_names[] = {"logs", "metrics", "events", "traces", "alerts"};
    
    printf("\n    Setting up simulated concurrent workload...");
    
    for (int i = 0; i < 5; i++) {
        tables[i] = table_create(table_names[i], 
            "CREATE TABLE workload (timestamp INTEGER, thread_id INTEGER, data TEXT(32))");
        REQUIRE(tables[i] != NULL, "Failed to create workload table");
    }
    
    // Simulate interleaved operations across tables
    const int CONCURRENT_OPS = 50000;
    clock_t start = clock();
    
    for (int i = 0; i < CONCURRENT_OPS; i++) {
        // Round-robin across tables
        Table* table = tables[i % 5];
        
        Value values[3];
        values[0] = value_integer(time(NULL) + i);
        values[1] = value_integer(i % 10); // Simulate thread IDs
        values[2] = value_text("concurrent_data");
        
        REQUIRE(table_append_row(table, values), "Concurrent simulation insert failed");
        value_destroy(&values[2]);
        
        if (i % 5000 == 0) {
            printf(".");
            fflush(stdout);
        }
    }
    
    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    double ops_per_sec = CONCURRENT_OPS / elapsed;
    
    printf("\n    Simulated concurrent results:");
    printf("\n      Total operations: %d", CONCURRENT_OPS);
    printf("\n      Time: %.3f seconds", elapsed);
    printf("\n      Throughput: %.0f ops/sec", ops_per_sec);
    
    // Verify all tables have data
    for (int i = 0; i < 5; i++) {
        REQUIRE(tables[i]->header->num_rows == CONCURRENT_OPS / 5, 
                "Uneven distribution across tables");
        table_close(tables[i]);
    }
    
    REQUIRE(ops_per_sec > 100000, "Should maintain >100K ops/sec in concurrent simulation");
    
    return true;
}

// Stress test: Schema with maximum columns
bool test_maximum_columns_stress(void) {
    cleanup_test_files();
    
    printf("\n    Testing maximum column schema...");
    
    // Build schema with maximum number of columns
    char schema[4096];
    strcpy(schema, "CREATE TABLE max_cols (");
    
    for (int i = 0; i < MAX_COLUMNS; i++) {
        char col_def[64];
        snprintf(col_def, sizeof(col_def), "col%d INTEGER", i);
        strcat(schema, col_def);
        if (i < MAX_COLUMNS - 1) {
            strcat(schema, ", ");
        }
    }
    strcat(schema, ")");
    
    Table* table = table_create("max_cols", schema);
    REQUIRE(table != NULL, "Failed to create table with maximum columns");
    REQUIRE(table->header->column_count == MAX_COLUMNS, "Column count mismatch");
    
    printf("\n    Created table with %d columns", MAX_COLUMNS);
    printf("\n    Row size: %u bytes", table->header->row_size);
    
    // Insert data with all columns
    Value* values = malloc(MAX_COLUMNS * sizeof(Value));
    REQUIRE(values != NULL, "Failed to allocate values array");
    
    for (int row = 0; row < 1000; row++) {
        for (int col = 0; col < MAX_COLUMNS; col++) {
            values[col] = value_integer(row * MAX_COLUMNS + col);
        }
        
        REQUIRE(table_append_row(table, values), "Failed to insert max column row");
    }
    
    printf("\n    Inserted 1000 rows with %d columns each", MAX_COLUMNS);
    REQUIRE(table->header->num_rows == 1000, "Row count mismatch");
    
    free(values);
    table_close(table);
    return true;
}

// Stress test: Large text field limits
bool test_large_text_stress(void) {
    cleanup_test_files();
    
    Table* table = table_create("large_text_stress", 
        "CREATE TABLE large_text_stress (id INTEGER, large_text TEXT(255))");
    REQUIRE(table != NULL, "Failed to create large text table");
    
    printf("\n    Testing large text field stress...");
    
    // Create maximum size text
    char max_text[256];
    for (int i = 0; i < 255; i++) {
        max_text[i] = 'A' + (i % 26);
    }
    max_text[255] = '\0';
    
    clock_t start = clock();
    
    // Insert many rows with large text
    for (int i = 0; i < 10000; i++) {
        Value values[2];
        values[0] = value_integer(i);
        values[1] = value_text(max_text);
        
        REQUIRE(table_append_row(table, values), "Large text insert failed");
        value_destroy(&values[1]);
        
        if (i % 1000 == 0) {
            printf(".");
            fflush(stdout);
        }
    }
    
    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    double rows_per_sec = 10000.0 / elapsed;
    
    printf("\n    Large text stress results:");
    printf("\n      Rows: 10000");
    printf("\n      Text size: 255 bytes per row");
    printf("\n      Time: %.3f seconds", elapsed);
    printf("\n      Throughput: %.0f rows/sec", rows_per_sec);
    printf("\n      Data volume: %.1f MB", 
           (double)(10000 * table->header->row_size) / (1024 * 1024));
    
    REQUIRE(rows_per_sec > 10000, "Should handle >10K rows/sec with large text");
    REQUIRE(table->header->num_rows == 10000, "Row count mismatch");
    
    table_close(table);
    return true;
}

int main(void) {
    printf("RistrettoDB Stress Test Suite\n");
    printf("=============================\n");
    printf("Testing performance under extreme conditions...\n\n");
    
    // High-volume stress tests
    TEST(large_dataset_insertion);
    TEST(memory_pressure_text);
    TEST(file_growth_stress);
    TEST(rapid_table_lifecycle);
    TEST(simulated_concurrent_load);
    TEST(maximum_columns_stress);
    TEST(large_text_stress);
    
    printf("\n=============================\n");
    printf("Stress Test Results:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\nSUCCESS: ALL STRESS TESTS PASSED!\n");
        printf("SUCCESS: Handles 1M+ row datasets\n");
        printf("SUCCESS: Memory management under pressure\n");
        printf("SUCCESS: File growth works correctly\n");
        printf("SUCCESS: Performance maintained under load\n");
        printf("SUCCESS: Maximum schema limits supported\n");
    } else {
        printf("\nERROR: %d STRESS TESTS FAILED\n", tests_failed);
        printf("Some performance claims may not hold under extreme load\n");
    }
    
    cleanup_test_files();
    
    return (tests_failed == 0) ? 0 : 1;
}