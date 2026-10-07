/*
** RistrettoDB Direct API Demo (Table V2)
**
** Disables the compatibility layer with RISTRETTO_NO_COMPATIBILITY_LAYER so
** only the prefixed ristretto_* names are visible — the recommended way to
** avoid any clash with your own `Table` / `Value` symbols.
**
** To compile and run:
**   make static
**   clang -O3 -Iembed -DRISTRETTO_NO_COMPATIBILITY_LAYER \
**       -o direct_api_demo examples/direct_api_demo.c lib/libristretto.a
**   ./direct_api_demo
*/

#define RISTRETTO_NO_COMPATIBILITY_LAYER
#include "ristretto.h"
#include <stdio.h>
#include <stdlib.h>

static void print_row(void *ctx, const RistrettoValue *row) {
    long *count = (long *)ctx;
    if (*count < 5) {
        printf("  id=%-6lld amount=%10.2f description=%s\n",
               (long long)row[0].value.integer,
               row[1].value.real,
               row[2].is_null ? "(null)" : row[2].value.text.data);
    }
    (*count)++;
}

int main(void) {
    printf("==============================================\n");
    printf("    RistrettoDB Direct API Demo (Table V2)\n");
    printf("==============================================\n");
    printf("Library Version: %s\n\n", ristretto_version());

    RistrettoTable *table = ristretto_table_create("transactions",
        "CREATE TABLE transactions (id INTEGER, amount REAL, description TEXT(64))");
    if (!table) {
        fprintf(stderr, "ERROR: Failed to create table\n");
        return 1;
    }
    printf("SUCCESS: table 'transactions' created\n");

    struct { int64_t id; double amount; const char *desc; } rows[] = {
        {1,  250.00,  "Grocery shopping"},
        {2,  -45.00,  "Gas station"},
        {3, 1200.00,  "Salary deposit"},
    };

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        RistrettoValue v[3];
        v[0] = ristretto_value_integer(rows[i].id);
        v[1] = ristretto_value_real(rows[i].amount);
        v[2] = ristretto_value_text(rows[i].desc);
        if (!ristretto_table_append_row(table, v)) {
            fprintf(stderr, "ERROR: insert %zu failed\n", i);
        }
        ristretto_value_destroy(&v[2]);
    }
    printf("SUCCESS: %zu transactions recorded\n\n", ristretto_table_get_row_count(table));

    printf("Transactions:\n");
    long count = 0;
    ristretto_table_select(table, print_row, &count);

    ristretto_table_close(table);
    printf("\nSUCCESS: Direct API demo completed.\n");
    return 0;
}
