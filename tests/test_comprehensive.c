#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include "table_v2.h"
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
    system("rm -rf data/");
}

// ========================================
// PROGRAMMING MANUAL EXAMPLE VALIDATION
// ========================================

// Test: Basic Table V2 setup from manual
bool test_table_v2_basic_setup(void) {
    cleanup_test_files();
    
    // From programming manual basic setup example
    Table* table = table_create("events", 
        "CREATE TABLE events (timestamp INTEGER, user_id INTEGER, event TEXT(32))");
    
    REQUIRE(table != NULL, "Table creation failed");
    REQUIRE(table->header->column_count == 3, "Wrong column count");
    REQUIRE(table->header->row_size == 8 + 8 + 32, "Wrong row size calculation");
    
    printf("\n    Table created with %u columns, %u bytes per row", 
           table->header->column_count, table->header->row_size);
    
    table_close(table);
    return true;
}

// Test: All value types from manual
bool test_all_value_types(void) {
    cleanup_test_files();
    
    Table* table = table_create("demo", 
        "CREATE TABLE demo (id INTEGER, name TEXT(50), score REAL)");
    REQUIRE(table != NULL, "Table creation failed");
    
    // Test all value creation functions from manual
    Value values[3];
    values[0] = value_integer(42);
    values[1] = value_text("John Doe");
    values[2] = value_real(95.5);
    
    REQUIRE(values[0].type == COL_TYPE_INTEGER, "Integer value type wrong");
    REQUIRE(values[0].value.integer == 42, "Integer value wrong");
    REQUIRE(!values[0].is_null, "Integer should not be null");
    
    REQUIRE(values[1].type == COL_TYPE_TEXT, "Text value type wrong");
    REQUIRE(strcmp(values[1].value.text.data, "John Doe") == 0, "Text value wrong");
    REQUIRE(values[1].value.text.length == 8, "Text length wrong");
    REQUIRE(!values[1].is_null, "Text should not be null");
    
    REQUIRE(values[2].type == COL_TYPE_REAL, "Real value type wrong");
    REQUIRE(values[2].value.real == 95.5, "Real value wrong");
    REQUIRE(!values[2].is_null, "Real should not be null");
    
    // Test insertion
    REQUIRE(table_append_row(table, values), "Row insertion failed");
    REQUIRE(table->header->num_rows == 1, "Row count wrong after insert");
    
    // Test null value
    Value null_val = value_null();
    REQUIRE(null_val.is_null, "Null value not marked as null");
    
    // Clean up text values as documented
    value_destroy(&values[1]);
    
    table_close(table);
    return true;
}

// Test: High-speed insertion from manual
bool test_high_speed_insertion(void) {
    cleanup_test_files();
    
    Table* table = table_create("access_log", 
        "CREATE TABLE access_log (timestamp INTEGER, ip TEXT(16), "
        "status INTEGER, bytes INTEGER)");
    REQUIRE(table != NULL, "Table creation failed");
    
    printf("\n    Starting high-speed insertion test...");
    clock_t start = clock();
    
    // Insert 10,000 log entries (scaled down from manual example)
    for (int i = 0; i < 10000; i++) {
        Value values[4];
        values[0] = value_integer(time(NULL) + i);
        values[1] = value_text("192.168.1.100");
        values[2] = value_integer(200);
        values[3] = value_integer(1024 + (i % 10000));
        
        REQUIRE(table_append_row(table, values), "Insert failed");
        value_destroy(&values[1]);
    }
    
    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    double rows_per_sec = 10000.0 / elapsed;
    
    printf("\n    Inserted 10,000 rows in %.3f seconds", elapsed);
    printf("\n    Throughput: %.0f rows/second", rows_per_sec);
    
    // Should achieve at least 100K rows/sec for this test
    REQUIRE(rows_per_sec > 100000, "Performance below minimum threshold");
    REQUIRE(table->header->num_rows == 10000, "Row count mismatch");
    
    table_close(table);
    return true;
}

// Test: Memory management best practices from manual
bool test_memory_management(void) {
    cleanup_test_files();
    
    Table* table = table_create("users", 
        "CREATE TABLE users (id INTEGER, name TEXT(64), email TEXT(128))");
    REQUIRE(table != NULL, "Table creation failed");
    
    // Test correct memory usage pattern from manual
    for (int i = 0; i < 100; i++) {
        Value values[3];
        values[0] = value_integer(i);
        values[1] = value_text("Alice Johnson");
        values[2] = value_text("alice@example.com");
        
        REQUIRE(table_append_row(table, values), "Insert failed");
        
        // Critical: Always destroy text values (as documented)
        value_destroy(&values[1]);
        value_destroy(&values[2]);
    }
    
    REQUIRE(table->header->num_rows == 100, "Wrong row count");
    
    table_close(table);
    return true;
}

// Query context structure for scanning tests
typedef struct {
    int total_events;
    int target_user_id;
    int matching_events;
} QueryContext;

// Callback function for query scanning tests
void count_events_callback(void* context, const Value* row) {
    QueryContext* qctx = (QueryContext*)context;
    qctx->total_events++;
    
    if (row[1].type == COL_TYPE_INTEGER && 
        row[1].value.integer == qctx->target_user_id) {
        qctx->matching_events++;
    }
}

// Test: Query and scanning from manual
bool test_query_scanning(void) {
    cleanup_test_files();
    
    Table* table = table_create("events", 
        "CREATE TABLE events (timestamp INTEGER, user_id INTEGER, event TEXT(32))");
    REQUIRE(table != NULL, "Table creation failed");
    
    // Insert test data
    for (int i = 0; i < 50; i++) {
        Value values[3];
        values[0] = value_integer(time(NULL) + i);
        values[1] = value_integer(i % 5); // 5 different users
        values[2] = value_text("test_event");
        
        REQUIRE(table_append_row(table, values), "Insert failed");
        value_destroy(&values[2]);
    }
    
    // Test table scanning with callback (from manual)
    QueryContext ctx = { .total_events = 0, .target_user_id = 2, .matching_events = 0 };
    
    REQUIRE(table_select(table, NULL, count_events_callback, &ctx), "Table scan failed");
    REQUIRE(ctx.total_events == 50, "Wrong total event count");
    REQUIRE(ctx.matching_events == 10, "Wrong matching event count"); // user_id 2 appears 10 times
    
    table_close(table);
    return true;
}

// ========================================
// SCHEMA DESIGN PATTERN VALIDATION
// ========================================

// Test: Time-series schema from manual
bool test_timeseries_schema(void) {
    cleanup_test_files();
    
    // From manual schema design patterns
    Table* table = table_create("sensors",
        "CREATE TABLE sensors ("
        "timestamp INTEGER, "
        "sensor_id INTEGER, "
        "value REAL, "
        "quality INTEGER"
        ")");
    
    REQUIRE(table != NULL, "Timeseries table creation failed");
    REQUIRE(table->header->column_count == 4, "Wrong column count");
    REQUIRE(table->header->row_size == 32, "Wrong row size for timeseries"); // 8+8+8+8
    
    // Insert sensor data
    for (int i = 0; i < 1000; i++) {
        Value values[4];
        values[0] = value_integer(time(NULL) + i);
        values[1] = value_integer(i % 10); // 10 sensors
        values[2] = value_real(20.0 + (i % 50) / 10.0); // Temperature readings
        values[3] = value_integer(95 + (i % 5)); // Quality 95-99
        
        REQUIRE(table_append_row(table, values), "Sensor data insert failed");
    }
    
    REQUIRE(table->header->num_rows == 1000, "Wrong sensor data count");
    
    table_close(table);
    return true;
}

// Test: Log entry schema from manual
bool test_log_schema(void) {
    cleanup_test_files();
    
    Table* table = table_create("logs",
        "CREATE TABLE logs ("
        "timestamp INTEGER, "
        "level INTEGER, "
        "component TEXT(16), "
        "message TEXT(128)"
        ")");
    
    REQUIRE(table != NULL, "Log table creation failed");
    REQUIRE(table->header->column_count == 4, "Wrong column count");
    REQUIRE(table->header->row_size == 160, "Wrong row size for logs"); // 8+8+16+128
    
    // Test different log levels
    const char* components[] = {"auth", "db", "web", "api"};
    const char* messages[] = {
        "User login successful",
        "Database connection established", 
        "HTTP request processed",
        "API call completed"
    };
    
    for (int i = 0; i < 100; i++) {
        Value values[4];
        values[0] = value_integer(time(NULL) + i);
        values[1] = value_integer(i % 4); // 4 log levels (DEBUG, INFO, WARN, ERROR)
        values[2] = value_text(components[i % 4]);
        values[3] = value_text(messages[i % 4]);
        
        REQUIRE(table_append_row(table, values), "Log insert failed");
        value_destroy(&values[2]);
        value_destroy(&values[3]);
    }
    
    REQUIRE(table->header->num_rows == 100, "Wrong log count");
    
    table_close(table);
    return true;
}

// ========================================
// ERROR HANDLING VALIDATION
// ========================================

// Test: Invalid schema handling
bool test_invalid_schema_handling(void) {
    cleanup_test_files();
    
    // Test invalid schema syntax
    Table* table1 = table_create("bad1", "INVALID SQL SYNTAX");
    REQUIRE(table1 == NULL, "Should reject invalid schema");
    
    // Test unsupported data type
    Table* table2 = table_create("bad2", "CREATE TABLE bad2 (id BLOB)");
    REQUIRE(table2 == NULL, "Should reject unsupported data type");
    
    // Test empty schema
    Table* table3 = table_create("bad3", "");
    REQUIRE(table3 == NULL, "Should reject empty schema");
    
    // Test too many columns (should work up to limit)
    char schema[2048];
    strcpy(schema, "CREATE TABLE many_cols (");
    for (int i = 0; i < MAX_COLUMNS; i++) {
        char col[32];
        snprintf(col, sizeof(col), "col%d INTEGER", i);
        strcat(schema, col);
        if (i < MAX_COLUMNS - 1) strcat(schema, ", ");
    }
    strcat(schema, ")");
    
    Table* table4 = table_create("many_cols", schema);
    REQUIRE(table4 != NULL, "Should handle maximum columns");
    REQUIRE(table4->header->column_count == MAX_COLUMNS, "Wrong column count for max test");
    
    table_close(table4);
    return true;
}

// Test: Table opening edge cases
bool test_table_opening_edge_cases(void) {
    cleanup_test_files();
    
    // Test opening non-existent table
    Table* table1 = table_open("nonexistent");
    REQUIRE(table1 == NULL, "Should fail to open non-existent table");
    
    // Create a table first
    Table* created_table = table_create("test_open", 
        "CREATE TABLE test_open (id INTEGER, data TEXT(32))");
    REQUIRE(created_table != NULL, "Failed to create test table");
    
    // Insert some data
    Value values[2];
    values[0] = value_integer(123);
    values[1] = value_text("test data");
    REQUIRE(table_append_row(created_table, values), "Failed to insert test data");
    value_destroy(&values[1]);
    
    table_close(created_table);
    
    // Now test reopening
    Table* reopened = table_open("test_open");
    REQUIRE(reopened != NULL, "Failed to reopen existing table");
    REQUIRE(reopened->header->num_rows == 1, "Wrong row count after reopening");
    REQUIRE(reopened->header->column_count == 2, "Wrong column count after reopening");
    
    table_close(reopened);
    return true;
}

// ========================================
// PERFORMANCE CLAIM VALIDATION
// ========================================

// Test: Validate performance claims from manual
bool test_performance_claims(void) {
    cleanup_test_files();
    
    Table* table = table_create("perf_test", 
        "CREATE TABLE perf_test (id INTEGER, data TEXT(8))");
    REQUIRE(table != NULL, "Performance test table creation failed");
    
    const int TEST_ROWS = 50000; // Scaled for reasonable test time
    
    printf("\n    Testing performance with %d rows...", TEST_ROWS);
    clock_t start = clock();
    
    for (int i = 0; i < TEST_ROWS; i++) {
        Value values[2];
        values[0] = value_integer(i);
        values[1] = value_text("testdata");
        
        REQUIRE(table_append_row(table, values), "Performance test insert failed");
        value_destroy(&values[1]);
    }
    
    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    double rows_per_sec = TEST_ROWS / elapsed;
    double ns_per_row = (elapsed * 1e9) / TEST_ROWS;
    
    printf("\n    Performance: %.0f rows/sec, %.0f ns/row", rows_per_sec, ns_per_row);
    
    // Validate performance claims from manual
    // Manual claims 4.6M rows/sec, we should get at least 1M for this test
    REQUIRE(rows_per_sec > 1000000, "Performance below claimed threshold");
    
    // Manual claims 215ns/row, we should be under 1000ns for this test
    REQUIRE(ns_per_row < 1000, "Latency above acceptable threshold");
    
    table_close(table);
    return true;
}

// ========================================
// REAL-WORLD SCENARIO TESTS
// ========================================

// Test: IoT telemetry system from manual
bool test_iot_telemetry_scenario(void) {
    cleanup_test_files();
    
    // Create sensor data table from manual example
    Table* sensor_data = table_create("sensor_data",
        "CREATE TABLE sensor_data ("
        "timestamp INTEGER, "
        "device_id INTEGER, "
        "temperature REAL, "
        "humidity REAL, "
        "battery_level INTEGER"
        ")");
    
    REQUIRE(sensor_data != NULL, "IoT sensor table creation failed");
    
    // Simulate data collection like manual example
    printf("\n    Simulating IoT data collection...");
    
    for (int i = 0; i < 1000; i++) {
        for (int device = 1; device <= 3; device++) {
            Value values[5];
            values[0] = value_integer(time(NULL) + i);
            values[1] = value_integer(device);
            values[2] = value_real(20.0 + (rand() % 200) / 10.0);  // 20-40°C
            values[3] = value_real(30.0 + (rand() % 500) / 10.0);  // 30-80%
            values[4] = value_integer(20 + (rand() % 80));          // 20-100%
            
            REQUIRE(table_append_row(sensor_data, values), "IoT data insert failed");
        }
    }
    
    REQUIRE(sensor_data->header->num_rows == 3000, "Wrong IoT data count"); // 1000 * 3 devices
    
    printf("\n    Collected %llu sensor readings", (unsigned long long)sensor_data->header->num_rows);
    
    table_close(sensor_data);
    return true;
}

// Test: Security audit system from manual
bool test_security_audit_scenario(void) {
    cleanup_test_files();
    
    // Create security events table from manual
    Table* security_events = table_create("security_events",
        "CREATE TABLE security_events ("
        "timestamp INTEGER, "
        "event_type INTEGER, "
        "user_id INTEGER, "
        "source_ip TEXT(16), "
        "description TEXT(128)"
        ")");
    
    REQUIRE(security_events != NULL, "Security events table creation failed");
    
    // Simulate security events like manual example
    const char* descriptions[] = {
        "User login successful",
        "Failed login attempt",
        "Privilege escalation attempt",
        "File access granted",
        "File access denied"
    };
    
    const char* ips[] = {
        "192.168.1.100",
        "192.168.1.101", 
        "10.0.0.50",
        "172.16.1.200"
    };
    
    for (int i = 0; i < 500; i++) {
        Value values[5];
        values[0] = value_integer(time(NULL) + i);
        values[1] = value_integer((i % 5) + 1); // Event types 1-5
        values[2] = value_integer(1000 + (i % 50)); // User IDs
        values[3] = value_text(ips[i % 4]);
        values[4] = value_text(descriptions[i % 5]);
        
        REQUIRE(table_append_row(security_events, values), "Security event insert failed");
        value_destroy(&values[3]);
        value_destroy(&values[4]);
    }
    
    REQUIRE(security_events->header->num_rows == 500, "Wrong security event count");
    
    table_close(security_events);
    return true;
}

// ========================================
// SCHEMA LIMITS AND EDGE CASES
// ========================================

// Test: Text field limits
bool test_text_field_limits(void) {
    cleanup_test_files();
    
    // Test maximum text field size
    Table* table = table_create("text_limits",
        "CREATE TABLE text_limits (id INTEGER, large_text TEXT(255))");
    
    REQUIRE(table != NULL, "Large text table creation failed");
    REQUIRE(table->header->columns[1].length == 255, "Text field size not set correctly");
    
    // Test inserting maximum size text
    char large_text[256];
    memset(large_text, 'A', 255);
    large_text[255] = '\0';
    
    Value values[2];
    values[0] = value_integer(1);
    values[1] = value_text(large_text);
    
    REQUIRE(table_append_row(table, values), "Large text insert failed");
    value_destroy(&values[1]);
    
    // Test text truncation for over-size text
    char oversized[300];
    memset(oversized, 'B', 299);
    oversized[299] = '\0';
    
    values[0] = value_integer(2);
    values[1] = value_text(oversized);
    
    REQUIRE(table_append_row(table, values), "Oversized text insert should succeed with truncation");
    value_destroy(&values[1]);
    
    table_close(table);
    return true;
}

// Test: File growth and remapping
bool test_file_growth(void) {
    cleanup_test_files();
    
    Table* table = table_create("growth_test", "CREATE TABLE growth_test (id INTEGER)");
    REQUIRE(table != NULL, "Growth test table creation failed");
    
    size_t initial_size = table->mapped_size;
    printf("\n    Initial file size: %zu bytes", initial_size);
    
    // Insert enough data to trigger file growth
    int rows_per_mb = (1024 * 1024) / table->header->row_size;
    int rows_to_insert = rows_per_mb + 1000; // Should trigger growth
    
    printf("\n    Inserting %d rows to trigger file growth...", rows_to_insert);
    
    for (int i = 0; i < rows_to_insert; i++) {
        Value values[1];
        values[0] = value_integer(i);
        
        REQUIRE(table_append_row(table, values), "Insert failed during growth test");
    }
    
    printf("\n    Final file size: %zu bytes", table->mapped_size);
    REQUIRE(table->mapped_size > initial_size, "File should have grown");
    REQUIRE(table->header->num_rows == rows_to_insert, "Row count mismatch after growth");
    
    table_close(table);
    return true;
}

int main(void) {
    printf("RistrettoDB Comprehensive Test Suite\n");
    printf("====================================\n");
    printf("Validating all programming manual claims...\n\n");
    
    // Programming manual example validation
    TEST(table_v2_basic_setup);
    TEST(all_value_types);
    TEST(high_speed_insertion);
    TEST(memory_management);
    TEST(query_scanning);
    
    // Schema design pattern validation
    TEST(timeseries_schema);
    TEST(log_schema);
    
    // Error handling validation
    TEST(invalid_schema_handling);
    TEST(table_opening_edge_cases);
    
    // Performance claim validation
    TEST(performance_claims);
    
    // Real-world scenario validation
    TEST(iot_telemetry_scenario);
    TEST(security_audit_scenario);
    
    // Schema limits and edge cases
    TEST(text_field_limits);
    TEST(file_growth);
    
    printf("\n====================================\n");
    printf("Test Results Summary:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\nSUCCESS: ALL TESTS PASSED!\n");
        printf("SUCCESS: All programming manual claims validated\n");
        printf("SUCCESS: Real-world scenarios working\n");
        printf("SUCCESS: Error handling robust\n");
        printf("SUCCESS: Performance claims verified\n");
    } else {
        printf("\nERROR: %d TESTS FAILED\n", tests_failed);
        printf("Some programming manual claims need attention\n");
    }
    
    cleanup_test_files();
    
    return (tests_failed == 0) ? 0 : 1;
}