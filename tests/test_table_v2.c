#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include "table_v2.h"

// Test result counting
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("Running test: %s ... ", #name); \
        tests_run++; \
        if (test_##name()) { \
            printf("PASS\n"); \
            tests_passed++; \
        } else { \
            printf("FAIL\n"); \
        } \
    } while(0)

// Cleanup function
void cleanup_test_files(void) {
    system("rm -rf data/");
}

// Test schema parsing
bool test_schema_parsing(void) {
    ColumnDesc columns[MAX_COLUMNS];
    uint32_t column_count, row_size;
    
    const char *schema = "CREATE TABLE users (id INTEGER, name TEXT(32), age INTEGER)";
    
    if (!table_parse_schema(schema, columns, &column_count, &row_size)) {
        return false;
    }
    
    if (column_count != 3) return false;
    if (row_size != 8 + 32 + 8) return false; // INTEGER + TEXT(32) + INTEGER
    
    // Check first column
    if (strcmp(columns[0].name, "id") != 0) return false;
    if (columns[0].type != COL_TYPE_INTEGER) return false;
    if (columns[0].length != 8) return false;
    if (columns[0].offset != 0) return false;
    
    // Check second column
    if (strcmp(columns[1].name, "name") != 0) return false;
    if (columns[1].type != COL_TYPE_TEXT) return false;
    if (columns[1].length != 32) return false;
    if (columns[1].offset != 8) return false;
    
    // Check third column
    if (strcmp(columns[2].name, "age") != 0) return false;
    if (columns[2].type != COL_TYPE_INTEGER) return false;
    if (columns[2].length != 8) return false;
    if (columns[2].offset != 40) return false;
    
    return true;
}

// Test table creation
bool test_table_creation(void) {
    cleanup_test_files();
    
    const char *schema = "CREATE TABLE test (id INTEGER, value REAL)";
    Table *table = table_create("test", schema);
    
    if (!table) return false;
    
    // Check header values
    if (table->header->column_count != 2) {
        table_close(table);
        return false;
    }
    
    if (table->header->row_size != 16) { // 8 + 8
        table_close(table);
        return false;
    }
    
    if (table->header->num_rows != 0) {
        table_close(table);
        return false;
    }
    
    table_close(table);
    return true;
}

// Test table opening
bool test_table_opening(void) {
    // Create a table first
    const char *schema = "CREATE TABLE persistent (id INTEGER, name TEXT(16))";
    Table *table = table_create("persistent", schema);
    if (!table) return false;
    
    // Add some data
    Value values[2];
    values[0] = value_integer(42);
    values[1] = value_text("hello");
    
    if (!table_append_row(table, values)) {
        value_destroy(&values[1]);
        table_close(table);
        return false;
    }
    
    value_destroy(&values[1]);
    table_close(table);
    
    // Reopen the table
    table = table_open("persistent");
    if (!table) return false;
    
    if (table->header->num_rows != 1) {
        table_close(table);
        return false;
    }
    
    if (table->header->column_count != 2) {
        table_close(table);
        return false;
    }
    
    table_close(table);
    return true;
}

// Test row insertion
bool test_row_insertion(void) {
    const char *schema = "CREATE TABLE insert_test (id INTEGER, name TEXT(20), score REAL)";
    Table *table = table_create("insert_test", schema);
    if (!table) return false;
    
    // Insert multiple rows
    for (int i = 0; i < 100; i++) {
        Value values[3];
        values[0] = value_integer(i);
        values[1] = value_text("test_user");
        values[2] = value_real(i * 1.5);
        
        if (!table_append_row(table, values)) {
            value_destroy(&values[1]);
            table_close(table);
            return false;
        }
        
        value_destroy(&values[1]);
    }
    
    if (table->header->num_rows != 100) {
        table_close(table);
        return false;
    }
    
    table_close(table);
    return true;
}

// Test row retrieval callback
static int selection_count = 0;
static void count_callback(void *ctx, const Value *row) {
    (void)ctx;
    (void)row;
    selection_count++;
}

static void validate_callback(void *ctx, const Value *row) {
    int *expected_id = (int *)ctx;
    
    // Check that we got the expected values
    if (row[0].type == COL_TYPE_INTEGER && row[0].value.integer == *expected_id) {
        selection_count++;
    }
}

// Test table selection
bool test_table_selection(void) {
    const char *schema = "CREATE TABLE select_test (id INTEGER, value REAL)";
    Table *table = table_create("select_test", schema);
    if (!table) return false;
    
    // Insert test data
    for (int i = 0; i < 50; i++) {
        Value values[2];
        values[0] = value_integer(i);
        values[1] = value_real(i * 2.0);
        
        if (!table_append_row(table, values)) {
            table_close(table);
            return false;
        }
    }
    
    // Test selection
    selection_count = 0;
    if (!table_select(table, NULL, count_callback, NULL)) {
        table_close(table);
        return false;
    }
    
    if (selection_count != 50) {
        table_close(table);
        return false;
    }
    
    table_close(table);
    return true;
}

// Test value utilities
bool test_value_utilities(void) {
    // Test integer value
    Value int_val = value_integer(12345);
    if (int_val.type != COL_TYPE_INTEGER) return false;
    if (int_val.value.integer != 12345) return false;
    if (int_val.is_null) return false;
    
    // Test real value
    Value real_val = value_real(3.14159);
    if (real_val.type != COL_TYPE_REAL) return false;
    if (real_val.value.real != 3.14159) return false;
    if (real_val.is_null) return false;
    
    // Test text value
    Value text_val = value_text("Hello, World!");
    if (text_val.type != COL_TYPE_TEXT) return false;
    if (text_val.is_null) return false;
    if (strcmp(text_val.value.text.data, "Hello, World!") != 0) return false;
    if (text_val.value.text.length != 13) return false;
    
    value_destroy(&text_val);
    
    // Test null value
    Value null_val = value_null();
    if (!null_val.is_null) return false;
    
    return true;
}

// Performance test for ultra-fast writes
bool test_performance(void) {
    const char *schema = "CREATE TABLE perf_test (id INTEGER, data TEXT(8))";
    Table *table = table_create("perf_test", schema);
    if (!table) return false;
    
    const int NUM_ROWS = 10000;
    struct timespec start, end;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < NUM_ROWS; i++) {
        Value values[2];
        values[0] = value_integer(i);
        values[1] = value_text("data");
        
        if (!table_append_row(table, values)) {
            value_destroy(&values[1]);
            table_close(table);
            return false;
        }
        
        value_destroy(&values[1]);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_nsec - start.tv_nsec) / 1e9;
    double rows_per_sec = NUM_ROWS / elapsed;
    double ns_per_row = (elapsed * 1e9) / NUM_ROWS;
    
    printf("\n  Performance: %.0f rows/sec, %.0f ns/row ", rows_per_sec, ns_per_row);
    
    table_close(table);
    
    // We should be able to insert at least 100K rows/sec
    return rows_per_sec > 100000;
}

// Test file growth
bool test_file_growth(void) {
    const char *schema = "CREATE TABLE growth_test (id INTEGER)";
    Table *table = table_create("growth_test", schema);
    if (!table) return false;
    
    size_t initial_size = table->mapped_size;
    
    // Insert enough rows to trigger growth
    int rows_per_mb = (1024 * 1024) / table->header->row_size;
    int rows_to_insert = rows_per_mb + 1000; // Should trigger growth
    
    for (int i = 0; i < rows_to_insert; i++) {
        Value values[1];
        values[0] = value_integer(i);
        
        if (!table_append_row(table, values)) {
            table_close(table);
            return false;
        }
    }
    
    // File should have grown
    bool grew = (table->mapped_size > initial_size);
    
    table_close(table);
    return grew;
}

int main(void) {
    printf("RistrettoDB Table V2 Test Suite\n");
    printf("===============================\n\n");
    
    TEST(schema_parsing);
    TEST(value_utilities);
    TEST(table_creation);
    TEST(table_opening);
    TEST(row_insertion);
    TEST(table_selection);
    TEST(file_growth);
    TEST(performance);
    
    printf("\n===============================\n");
    printf("Tests passed: %d/%d\n", tests_passed, tests_run);
    
    cleanup_test_files();
    
    return (tests_passed == tests_run) ? 0 : 1;
}
