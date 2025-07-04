#include "ristretto.h"
#include <stdio.h>

int main(void) {
    printf("Testing RistrettoDB Embedded\n");
    printf("Version: %s\n", ristretto_version());
    
    // Test Original SQL API
    printf("\n--- Testing Original SQL API ---\n");
    RistrettoDB* db = ristretto_open("embedded_test.db");
    if (db) {
        printf("✅ Database opened successfully\n");
        
        RistrettoResult result = ristretto_exec(db, "CREATE TABLE test (id INTEGER, name TEXT)");
        if (result == RISTRETTO_OK) {
            printf("✅ Table created successfully\n");
        } else {
            printf("❌ Table creation failed: %s\n", ristretto_error_string(result));
        }
        
        ristretto_close(db);
    } else {
        printf("❌ Failed to open database\n");
    }
    
    // Test Table V2 API
    printf("\n--- Testing Table V2 API ---\n");
    RistrettoTable* table = ristretto_table_create("v2_test", 
        "CREATE TABLE v2_test (id INTEGER, value REAL, name TEXT(32))");
    
    if (table) {
        printf("✅ V2 table created successfully\n");
        
        // Insert a test row
        RistrettoValue values[3];
        values[0] = ristretto_value_integer(1);
        values[1] = ristretto_value_real(123.45);
        values[2] = ristretto_value_text("test_name");
        
        if (ristretto_table_append_row(table, values)) {
            printf("✅ Row inserted successfully\n");
            printf("Row count: %zu\n", ristretto_table_get_row_count(table));
        } else {
            printf("❌ Row insertion failed\n");
        }
        
        ristretto_value_destroy(&values[2]);
        ristretto_table_close(table);
    } else {
        printf("❌ Failed to create V2 table\n");
    }
    
    printf("\n🎉 Embedded test completed!\n");
    return 0;
}