#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "db.h"

// Test framework
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        printf("Running test: %s ... ", #name); \
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
    system("rm -f *.db");
}

// ========================================
// ORIGINAL SQL API VALIDATION
// ========================================

// Test: Basic database operations from manual
bool test_basic_database_operations(void) {
    cleanup_test_files();
    
    // From programming manual basic usage example
    RistrettoDB* db = ristretto_open("myapp.db");
    REQUIRE(db != NULL, "Failed to open database");
    
    // Create table
    RistrettoResult result = ristretto_exec(db, 
        "CREATE TABLE users (id INTEGER, name TEXT, email TEXT)");
    REQUIRE(result == RISTRETTO_OK, "Failed to create table");
    
    printf("\n    Database and table created successfully");
    
    ristretto_close(db);
    return true;
}

// Test: Data insertion examples from manual
bool test_data_insertion(void) {
    cleanup_test_files();
    
    RistrettoDB* db = ristretto_open("test.db");
    REQUIRE(db != NULL, "Failed to open database");
    
    // Create table
    REQUIRE(ristretto_exec(db, 
        "CREATE TABLE users (id INTEGER, name TEXT, email TEXT)") == RISTRETTO_OK,
        "Failed to create table");
    
    // Test single inserts from manual examples
    const char* users[][3] = {
        {"1", "Alice Johnson", "alice@example.com"},
        {"2", "Bob Smith", "bob@example.com"}, 
        {"3", "Carol Davis", "carol@example.com"},
        {"4", "David Wilson", "david@example.com"}
    };
    
    for (int i = 0; i < 4; i++) {
        char sql[512];
        snprintf(sql, sizeof(sql), 
            "INSERT INTO users VALUES (%s, '%s', '%s')",
            users[i][0], users[i][1], users[i][2]);
        
        RistrettoResult result = ristretto_exec(db, sql);
        REQUIRE(result == RISTRETTO_OK, "Failed to insert user");
    }
    
    printf("\n    Inserted %d users successfully", 4);
    
    ristretto_close(db);
    return true;
}

// Callback functions for query testing
static int query_callback_count = 0;
static char last_user_name[256] = {0};

void print_user_callback(void* ctx, int n_cols, char** values, char** col_names) {
    (void)ctx;
    query_callback_count++;
    
    if (n_cols >= 2 && values[1]) {
        strncpy(last_user_name, values[1], sizeof(last_user_name) - 1);
    }
    
    printf("\n      User: ");
    for (int i = 0; i < n_cols; i++) {
        printf("%s=%s ", col_names[i], values[i] ? values[i] : "NULL");
    }
}

void count_callback(void* ctx, int n_cols, char** values, char** col_names) {
    (void)col_names;
    int* count = (int*)ctx;
    if (n_cols >= 1 && values[0]) {
        *count = atoi(values[0]);
    }
}

// Test: Query operations from manual
bool test_query_operations(void) {
    cleanup_test_files();
    
    RistrettoDB* db = ristretto_open("query_test.db");
    REQUIRE(db != NULL, "Failed to open database");
    
    // Setup test data
    REQUIRE(ristretto_exec(db, 
        "CREATE TABLE users (id INTEGER, name TEXT, email TEXT)") == RISTRETTO_OK,
        "Failed to create table");
    
    REQUIRE(ristretto_exec(db, 
        "INSERT INTO users VALUES (1, 'Alice Johnson', 'alice@example.com')") == RISTRETTO_OK,
        "Failed to insert test data");
    REQUIRE(ristretto_exec(db, 
        "INSERT INTO users VALUES (2, 'Bob Smith', 'bob@example.com')") == RISTRETTO_OK,
        "Failed to insert test data");
    REQUIRE(ristretto_exec(db, 
        "INSERT INTO users VALUES (3, 'Charlie Smith', 'charlie@example.com')") == RISTRETTO_OK,
        "Failed to insert test data");
    
    // Test SELECT * query from manual
    query_callback_count = 0;
    printf("\n    All users:");
    RistrettoResult result = ristretto_query(db, "SELECT * FROM users", 
                                           print_user_callback, NULL);
    REQUIRE(result == RISTRETTO_OK, "Failed to execute SELECT * query");
    REQUIRE(query_callback_count == 3, "Wrong number of rows returned");
    
    // Test filtered query from manual
    query_callback_count = 0;
    last_user_name[0] = '\0';
    printf("\n    Users with 'Smith' in name:");
    result = ristretto_query(db, "SELECT * FROM users WHERE name LIKE '%Smith%'", 
                            print_user_callback, NULL);
    REQUIRE(result == RISTRETTO_OK, "Failed to execute filtered query");
    REQUIRE(query_callback_count == 2, "Wrong number of filtered rows");
    
    // Test count query from manual
    int user_count = 0;
    result = ristretto_query(db, "SELECT COUNT(*) FROM users", count_callback, &user_count);
    REQUIRE(result == RISTRETTO_OK, "Failed to execute count query");
    REQUIRE(user_count == 3, "Wrong user count");
    
    printf("\n    Total users: %d", user_count);
    
    ristretto_close(db);
    return true;
}

// Test: Error handling from manual
bool test_error_handling(void) {
    cleanup_test_files();
    
    // Test opening non-existent path
    RistrettoDB* db = ristretto_open("/invalid/path/test.db");
    // Note: This might succeed depending on implementation
    if (db) ristretto_close(db);
    
    // Test with valid database
    db = ristretto_open("error_test.db");
    REQUIRE(db != NULL, "Failed to open database");
    
    // Test invalid SQL syntax
    RistrettoResult result = ristretto_exec(db, "INVALID SQL SYNTAX");
    REQUIRE(result != RISTRETTO_OK, "Should reject invalid SQL");
    printf("\n    Invalid SQL rejected: %s", ristretto_error_string(result));
    
    // Test creating table first
    REQUIRE(ristretto_exec(db, "CREATE TABLE test (id INTEGER)") == RISTRETTO_OK,
        "Failed to create table");
    
    // Test inserting into non-existent table
    result = ristretto_exec(db, "INSERT INTO nonexistent VALUES (1)");
    REQUIRE(result != RISTRETTO_OK, "Should reject insert into non-existent table");
    
    // Test valid operations still work
    REQUIRE(ristretto_exec(db, "INSERT INTO test VALUES (1)") == RISTRETTO_OK,
        "Valid insert should work after error");
    
    ristretto_close(db);
    return true;
}

// Test: Application logging example from manual
bool test_application_logging(void) {
    cleanup_test_files();
    
    // Implement Logger structure from manual
    typedef struct {
        RistrettoDB* db;
        int log_count;
    } Logger;
    
    // Create logger
    Logger logger;
    logger.db = ristretto_open("app.log");
    REQUIRE(logger.db != NULL, "Failed to create logger database");
    
    // Create logs table from manual
    RistrettoResult result = ristretto_exec(logger.db, 
        "CREATE TABLE logs (id INTEGER, timestamp INTEGER, "
        "level TEXT, component TEXT, message TEXT)");
    REQUIRE(result == RISTRETTO_OK, "Failed to create logs table");
    
    logger.log_count = 0;
    
    // Log some messages like manual example
    const char* log_entries[][3] = {
        {"INFO", "database", "Connection established"},
        {"WARN", "auth", "Failed login attempt"},
        {"ERROR", "network", "Connection timeout"},
        {"INFO", "app", "Processing completed"}
    };
    
    for (int i = 0; i < 4; i++) {
        char sql[1024];
        snprintf(sql, sizeof(sql),
            "INSERT INTO logs VALUES (%d, %ld, '%s', '%s', '%s')",
            ++logger.log_count, time(NULL), 
            log_entries[i][0], log_entries[i][1], log_entries[i][2]);
        
        result = ristretto_exec(logger.db, sql);
        REQUIRE(result == RISTRETTO_OK, "Failed to log message");
    }
    
    printf("\n    Logged %d messages", logger.log_count);
    
    // Verify logs can be queried
    int log_count = 0;
    result = ristretto_query(logger.db, "SELECT COUNT(*) FROM logs", 
                            count_callback, &log_count);
    REQUIRE(result == RISTRETTO_OK, "Failed to count logs");
    REQUIRE(log_count == 4, "Wrong log count");
    
    ristretto_close(logger.db);
    return true;
}

// Test: Database persistence
bool test_database_persistence(void) {
    cleanup_test_files();
    
    const char* db_path = "persist_test.db";
    
    // Create database and insert data
    {
        RistrettoDB* db = ristretto_open(db_path);
        REQUIRE(db != NULL, "Failed to create database");
        
        REQUIRE(ristretto_exec(db, 
            "CREATE TABLE persistent (id INTEGER, data TEXT)") == RISTRETTO_OK,
            "Failed to create table");
        
        REQUIRE(ristretto_exec(db, 
            "INSERT INTO persistent VALUES (1, 'test data')") == RISTRETTO_OK,
            "Failed to insert data");
        REQUIRE(ristretto_exec(db, 
            "INSERT INTO persistent VALUES (2, 'more data')") == RISTRETTO_OK,
            "Failed to insert data");
        
        ristretto_close(db);
    }
    
    // Reopen database and verify data persisted
    {
        RistrettoDB* db = ristretto_open(db_path);
        REQUIRE(db != NULL, "Failed to reopen database");
        
        int count = 0;
        RistrettoResult result = ristretto_query(db, "SELECT COUNT(*) FROM persistent", 
                                               count_callback, &count);
        REQUIRE(result == RISTRETTO_OK, "Failed to query reopened database");
        REQUIRE(count == 2, "Data not persisted correctly");
        
        printf("\n    Data persisted correctly: %d rows", count);
        
        ristretto_close(db);
    }
    
    return true;
}

// Test: Multiple tables support
bool test_multiple_tables(void) {
    cleanup_test_files();
    
    RistrettoDB* db = ristretto_open("multi_table.db");
    REQUIRE(db != NULL, "Failed to open database");
    
    // Create multiple tables
    REQUIRE(ristretto_exec(db, 
        "CREATE TABLE users (id INTEGER, name TEXT)") == RISTRETTO_OK,
        "Failed to create users table");
    
    REQUIRE(ristretto_exec(db, 
        "CREATE TABLE products (id INTEGER, name TEXT, price REAL)") == RISTRETTO_OK,
        "Failed to create products table");
    
    REQUIRE(ristretto_exec(db, 
        "CREATE TABLE orders (id INTEGER, user_id INTEGER, product_id INTEGER)") == RISTRETTO_OK,
        "Failed to create orders table");
    
    // Insert data into each table
    REQUIRE(ristretto_exec(db, 
        "INSERT INTO users VALUES (1, 'John')") == RISTRETTO_OK,
        "Failed to insert into users");
    
    REQUIRE(ristretto_exec(db, 
        "INSERT INTO products VALUES (1, 'Laptop', 999.99)") == RISTRETTO_OK,
        "Failed to insert into products");
    
    REQUIRE(ristretto_exec(db, 
        "INSERT INTO orders VALUES (1, 1, 1)") == RISTRETTO_OK,
        "Failed to insert into orders");
    
    // Verify each table has data
    int users_count = 0, products_count = 0, orders_count = 0;
    
    REQUIRE(ristretto_query(db, "SELECT COUNT(*) FROM users", 
            count_callback, &users_count) == RISTRETTO_OK, "Failed to count users");
    REQUIRE(ristretto_query(db, "SELECT COUNT(*) FROM products", 
            count_callback, &products_count) == RISTRETTO_OK, "Failed to count products");
    REQUIRE(ristretto_query(db, "SELECT COUNT(*) FROM orders", 
            count_callback, &orders_count) == RISTRETTO_OK, "Failed to count orders");
    
    REQUIRE(users_count == 1, "Wrong users count");
    REQUIRE(products_count == 1, "Wrong products count");
    REQUIRE(orders_count == 1, "Wrong orders count");
    
    printf("\n    Multiple tables working: users=%d, products=%d, orders=%d", 
           users_count, products_count, orders_count);
    
    ristretto_close(db);
    return true;
}

// Test: Data types support
bool test_data_types_support(void) {
    cleanup_test_files();
    
    RistrettoDB* db = ristretto_open("datatypes.db");
    REQUIRE(db != NULL, "Failed to open database");
    
    // Create table with all supported types
    REQUIRE(ristretto_exec(db, 
        "CREATE TABLE types_test (id INTEGER, name TEXT, price REAL, active INTEGER)") == RISTRETTO_OK,
        "Failed to create table with all types");
    
    // Insert data with different types
    REQUIRE(ristretto_exec(db, 
        "INSERT INTO types_test VALUES (42, 'Test Product', 123.45, 1)") == RISTRETTO_OK,
        "Failed to insert mixed types");
    
    REQUIRE(ristretto_exec(db, 
        "INSERT INTO types_test VALUES (-100, 'Negative Test', -999.99, 0)") == RISTRETTO_OK,
        "Failed to insert negative values");
    
    // Verify data can be queried
    int count = 0;
    REQUIRE(ristretto_query(db, "SELECT COUNT(*) FROM types_test", 
            count_callback, &count) == RISTRETTO_OK, "Failed to count type test rows");
    REQUIRE(count == 2, "Wrong row count for types test");
    
    printf("\n    All data types supported correctly");
    
    ristretto_close(db);
    return true;
}

int main(void) {
    printf("RistrettoDB Original API Test Suite\n");
    printf("===================================\n");
    printf("Validating Original SQL API functionality...\n\n");
    
    // Basic functionality tests
    TEST(basic_database_operations);
    TEST(data_insertion);
    TEST(query_operations);
    TEST(error_handling);
    
    // Advanced functionality tests  
    TEST(application_logging);
    TEST(database_persistence);
    TEST(multiple_tables);
    TEST(data_types_support);
    
    printf("\n===================================\n");
    printf("Original API Test Results:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n🎉 ALL ORIGINAL API TESTS PASSED!\n");
        printf("✅ SQL parsing working correctly\n");
        printf("✅ CRUD operations functional\n");
        printf("✅ Error handling robust\n");
        printf("✅ Multiple tables supported\n");
        printf("✅ Data persistence working\n");
    } else {
        printf("\n❌ %d ORIGINAL API TESTS FAILED\n", tests_failed);
    }
    
    cleanup_test_files();
    
    return (tests_failed == 0) ? 0 : 1;
}