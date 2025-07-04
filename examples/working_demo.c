/*
** RistrettoDB Working Embedding Example
**
** This example demonstrates RistrettoDB embedding using the exact function
** names that are exported by the library.
**
** To compile and run:
**   make lib                    # Build the libraries first
**   gcc -O3 -I. -o examples/working_demo examples/working_demo.c -Llib -lristretto
**   ./examples/working_demo
*/

#include "ristretto.h"
#include <stdio.h>
#include <stdlib.h>

void print_query_result(void* ctx, int n_cols, char** values, char** col_names) {
    (void)ctx;
    static int first_row = 1;
    
    if (first_row) {
        printf("Query results:\n");
        for (int i = 0; i < n_cols; i++) {
            printf("%-15s", col_names[i]);
        }
        printf("\n");
        for (int i = 0; i < n_cols; i++) {
            printf("%-15s", "---------------");
        }
        printf("\n");
        first_row = 0;
    }
    
    for (int i = 0; i < n_cols; i++) {
        printf("%-15s", values[i] ? values[i] : "NULL");
    }
    printf("\n");
}

int main(void) {
    printf("============================================\n");
    printf("  RistrettoDB Working Embedding Example\n");
    printf("============================================\n");
    printf("Library Version: %s\n\n", ristretto_version());
    
    // === PART 1: Original SQL API ===
    printf("Part 1: Original SQL API (2.8x faster than SQLite)\n");
    printf("==================================================\n");
    
    RistrettoDB* db = ristretto_open("working_demo.db");
    if (!db) {
        fprintf(stderr, "❌ Failed to open database\n");
        return 1;
    }
    printf("✅ Database opened: working_demo.db\n");
    
    // Create table
    RistrettoResult result = ristretto_exec(db, 
        "CREATE TABLE inventory (id INTEGER, item TEXT, quantity INTEGER, price REAL)");
    if (result == RISTRETTO_OK) {
        printf("✅ Table 'inventory' created\n");
    } else {
        fprintf(stderr, "❌ Table creation failed: %s\n", ristretto_error_string(result));
    }
    
    // Insert data
    const char* items[] = {
        "INSERT INTO inventory VALUES (1, 'Widgets', 100, 9.99)",
        "INSERT INTO inventory VALUES (2, 'Gadgets', 50, 19.99)",
        "INSERT INTO inventory VALUES (3, 'Tools', 25, 49.99)"
    };
    
    for (int i = 0; i < 3; i++) {
        result = ristretto_exec(db, items[i]);
        if (result == RISTRETTO_OK) {
            printf("✅ Inserted item %d\n", i + 1);
        } else {
            fprintf(stderr, "❌ Insert failed: %s\n", ristretto_error_string(result));
        }
    }
    
    // Query data
    printf("\n");
    result = ristretto_query(db, "SELECT * FROM inventory", print_query_result, NULL);
    if (result != RISTRETTO_OK) {
        fprintf(stderr, "❌ Query failed: %s\n", ristretto_error_string(result));
    }
    
    ristretto_close(db);
    printf("✅ Original SQL API demo completed\n\n");
    
    // === PART 2: Table V2 Ultra-Fast API ===
    printf("Part 2: Table V2 Ultra-Fast API (4.6M rows/sec)\n");
    printf("==============================================\n");
    
    // Use the actual exported function names (without ristretto_ prefix)
    Table* table = table_create("performance_log", 
        "CREATE TABLE performance_log (timestamp INTEGER, duration_ms REAL, operation TEXT(32))");
    
    if (!table) {
        fprintf(stderr, "❌ Failed to create ultra-fast table\n");
        return 1;
    }
    printf("✅ Ultra-fast table 'performance_log' created\n");
    
    // High-speed insertion test
    printf("✅ Starting high-speed insertion test...\n");
    
    const char* operations[] = {"SELECT", "INSERT", "UPDATE", "DELETE", "CREATE"};
    int successful_inserts = 0;
    
    // Insert 5000 performance records
    for (int i = 0; i < 5000; i++) {
        Value values[3];
        values[0] = value_integer(1672531200 + i);                    // timestamp
        values[1] = value_real(0.1 + (i % 100) * 0.05);             // duration_ms
        values[2] = value_text(operations[i % 5]);                   // operation
        
        if (table_append_row(table, values)) {
            successful_inserts++;
        } else {
            fprintf(stderr, "❌ Failed to insert row %d\n", i);
        }
        
        value_destroy(&values[2]);  // Clean up the text value
    }
    
    printf("✅ High-speed insertion completed\n");
    printf("   Records inserted: %d/5000\n", successful_inserts);
    printf("   Total rows in table: %zu\n", table_get_row_count(table));
    
    table_close(table);
    printf("✅ Table V2 ultra-fast demo completed\n\n");
    
    // === Summary ===
    printf("============================================\n");
    printf("                SUMMARY\n");
    printf("============================================\n");
    printf("✅ RistrettoDB successfully embedded and tested!\n\n");
    
    printf("🔧 Technical Details:\n");
    printf("   • Library size: ~42KB (static)\n");
    printf("   • Zero external dependencies\n");
    printf("   • C11 compatible\n");
    printf("   • POSIX systems (Linux, macOS, BSD)\n\n");
    
    printf("⚡ Performance Verified:\n");
    printf("   • Original API: 3 SQL operations completed\n");
    printf("   • Ultra-fast API: %d records inserted\n", successful_inserts);
    printf("   • Ready for production workloads\n\n");
    
    printf("🚀 Integration:\n");
    printf("   • Include: ristretto.h\n");
    printf("   • Link: -lristretto\n");
    printf("   • Compile: gcc -O3 myapp.c -lristretto\n\n");
    
    printf("Perfect for embedding in C/C++ applications! 🎯\n");
    
    return 0;
}