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
    // Each row starts with the NULL bitmap, then INTEGER + TEXT(32) + INTEGER.
    if (row_size != NULL_BITMAP_BYTES + 8 + 32 + 8) return false;

    // Check first column
    if (strcmp(columns[0].name, "id") != 0) return false;
    if (columns[0].type != COL_TYPE_INTEGER) return false;
    if (columns[0].length != 8) return false;
    if (columns[0].offset != NULL_BITMAP_BYTES) return false;

    // Check second column
    if (strcmp(columns[1].name, "name") != 0) return false;
    if (columns[1].type != COL_TYPE_TEXT) return false;
    if (columns[1].length != 32) return false;
    if (columns[1].offset != NULL_BITMAP_BYTES + 8) return false;

    // Check third column
    if (strcmp(columns[2].name, "age") != 0) return false;
    if (columns[2].type != COL_TYPE_INTEGER) return false;
    if (columns[2].length != 8) return false;
    if (columns[2].offset != NULL_BITMAP_BYTES + 40) return false;

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
    
    if (table->header->row_size != NULL_BITMAP_BYTES + 16) { // bitmap + 8 + 8
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
    if (!table_select(table, count_callback, NULL)) {
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

// #6 - table_append_row_n rejects a wrong value count and writes nothing.
bool test_append_row_count_guard(void) {
    table_close(table_create("guard", "CREATE TABLE guard (a INTEGER, b INTEGER, c INTEGER)"));
    Table *t = table_open("guard");
    if (!t) return false;

    Value vals[3] = { value_integer(1), value_integer(2), value_integer(3) };
    bool ok = true;
    // Too few / too many must fail and not change the row count.
    ok &= (table_append_row_n(t, vals, 2) == false);
    ok &= (table_append_row_n(t, vals, 4) == false);
    ok &= (table_get_row_count(t) == 0);
    // Correct count succeeds.
    ok &= (table_append_row_n(t, vals, 3) == true);
    ok &= (table_get_row_count(t) == 1);
    table_close(t);
    return ok;
}

// #5 - RDB_CREATE_NEW refuses to clobber an existing file; a custom base_dir
// is created and the .rdb lands there.
bool test_create_modes_and_basedir(void) {
    bool ok = true;
    Table *t = table_create_ex("modes", "CREATE TABLE modes (id INTEGER)",
                               "data", RDB_CREATE_NEW);
    ok &= (t != NULL);
    if (t) table_close(t);
    // Second CREATE_NEW on the now-existing file must fail.
    Table *dup = table_create_ex("modes", "CREATE TABLE modes (id INTEGER)",
                                 "data", RDB_CREATE_NEW);
    ok &= (dup == NULL);
    if (dup) table_close(dup);

    // Custom base_dir is created and used.
    system("rm -rf data_alt");
    Table *c = table_create_ex("custom", "CREATE TABLE custom (id INTEGER)",
                               "data_alt", RDB_CREATE_OR_TRUNCATE);
    ok &= (c != NULL);
    if (c) table_close(c);
    struct stat st;
    ok &= (stat("data_alt/custom.rdb", &st) == 0);
    system("rm -rf data_alt");
    return ok;
}

// #8 - advisory flock: a second open of the same live table returns NULL.
bool test_flock_second_open(void) {
    table_close(table_create("locked", "CREATE TABLE locked (id INTEGER)"));
    Table *a = table_open("locked");
    if (!a) return false;
    Table *b = table_open("locked"); // distinct handle, same file, a still open
    bool ok = (b == NULL);
    if (b) table_close(b);
    table_close(a);
    // After release, open succeeds again.
    Table *c = table_open("locked");
    ok &= (c != NULL);
    if (c) table_close(c);
    return ok;
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
    TEST(append_row_count_guard);
    TEST(create_modes_and_basedir);
    TEST(flock_second_open);
    
    printf("\n===============================\n");
    printf("Tests passed: %d/%d\n", tests_passed, tests_run);
    
    cleanup_test_files();
    
    return (tests_passed == tests_run) ? 0 : 1;
}
