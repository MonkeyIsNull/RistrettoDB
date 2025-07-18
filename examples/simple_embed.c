/*
** Simple RistrettoDB Embedding Example
**
** This example shows how to embed RistrettoDB in your C application.
** It demonstrates both the Original SQL API and Table V2 API.
**
** To compile and run:
**   make lib                    # Build the libraries first
**   gcc -O3 -o simple_embed examples/simple_embed.c -Llib -lristretto
**   ./simple_embed
*/

#include "ristretto.h"
#include <stdio.h>
#include <stdlib.h>

void simple_callback(void* ctx, int n_cols, char** values, char** col_names) {
    (void)ctx;
    for (int i = 0; i < n_cols; i++) {
        printf("%s: %s%s", col_names[i], values[i] ? values[i] : "NULL", 
               i < n_cols - 1 ? ", " : "\n");
    }
}

int main(void) {
    printf("=== RistrettoDB Simple Embedding Example ===\n");
    printf("Version: %s\n\n", ristretto_version());
    
    // Example 1: Original SQL API
    printf("--- Original SQL API (2.8x faster than SQLite) ---\n");
    
    RistrettoDB* db = ristretto_open("simple_example.db");
    if (!db) {
        fprintf(stderr, "Failed to open database\n");
        return 1;
    }
    
    // Create a table
    RistrettoResult result = ristretto_exec(db, 
        "CREATE TABLE employees (id INTEGER, name TEXT, salary REAL)");
    if (result != RISTRETTO_OK) {
        fprintf(stderr, "Failed to create table: %s\n", ristretto_error_string(result));
        ristretto_close(db);
        return 1;
    }
    
    // Insert some data
    const char* employees[] = {
        "INSERT INTO employees VALUES (1, 'Alice Johnson', 75000.0)",
        "INSERT INTO employees VALUES (2, 'Bob Smith', 68000.0)", 
        "INSERT INTO employees VALUES (3, 'Carol Davis', 82000.0)"
    };
    
    for (int i = 0; i < 3; i++) {
        result = ristretto_exec(db, employees[i]);
        if (result != RISTRETTO_OK) {
            fprintf(stderr, "Failed to insert data: %s\n", ristretto_error_string(result));
        }
    }
    
    // Query the data
    printf("Employees:\n");
    result = ristretto_query(db, "SELECT * FROM employees", simple_callback, NULL);
    if (result != RISTRETTO_OK) {
        fprintf(stderr, "Query failed: %s\n", ristretto_error_string(result));
    }
    
    ristretto_close(db);
    
    // Example 2: Table V2 Ultra-Fast API  
    printf("\n--- Table V2 API (4.6M rows/sec, 4.57x faster than SQLite) ---\n");
    
    RistrettoTable* table = ristretto_table_create("metrics", 
        "CREATE TABLE metrics (timestamp INTEGER, cpu_usage REAL, memory_mb INTEGER, process TEXT(32))");
    
    if (!table) {
        fprintf(stderr, "Failed to create V2 table\n");
        return 1;
    }
    
    // Insert high-speed data
    printf("Inserting 1000 metric records...\n");
    for (int i = 0; i < 1000; i++) {
        RistrettoValue values[4];
        values[0] = ristretto_value_integer(1672531200 + i);  // timestamp
        values[1] = ristretto_value_real(15.5 + (i % 50));   // cpu_usage
        values[2] = ristretto_value_integer(512 + (i % 200)); // memory_mb
        values[3] = ristretto_value_text("process_name");      // process
        
        if (!ristretto_table_append_row(table, values)) {
            fprintf(stderr, "Failed to insert row %d\n", i);
            break;
        }
        
        ristretto_value_destroy(&values[3]);
    }
    
    printf("Total rows inserted: %zu\n", ristretto_table_get_row_count(table));
    
    ristretto_table_close(table);
    
    printf("\nSUCCESS: Embedding example completed successfully!\n");
    printf("\nKey advantages of RistrettoDB:\n");
    printf("• Zero dependencies - just link the library\n");
    printf("• Small footprint - ~42KB static library\n");
    printf("• High performance - 2.8x to 4.57x faster than SQLite\n");
    printf("• Simple API - SQLite-inspired, easy to learn\n");
    printf("• Dual engines - choose the right tool for your use case\n");
    
    return 0;
}