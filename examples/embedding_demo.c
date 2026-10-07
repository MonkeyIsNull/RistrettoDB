/*
** RistrettoDB Embedding Demonstration (Table V2)
**
** A fuller embedding walkthrough: create a fixed-schema table, append rows
** (including a NULL field), flush durably, and scan the data back. NULLs now
** persist and round-trip as NULL (keyed off is_null).
**
** To compile and run:
**   make static
**   clang -O3 -Iembed -o embedding_demo examples/embedding_demo.c lib/libristretto.a
**   ./embedding_demo
*/

#include "ristretto.h"
#include <stdio.h>
#include <stdlib.h>

static void print_sensor(void *ctx, const RistrettoValue *row) {
    (void)ctx;
    printf("  %-18s reading=", row[0].is_null ? "(null)" : row[0].value.text.data);
    if (row[1].is_null) {
        printf("NULL\n");
    } else {
        printf("%.2f\n", row[1].value.real);
    }
}

int main(void) {
    printf("=== RistrettoDB Embedding Demonstration (Table V2) ===\n");
    printf("Version: %s\n\n", ristretto_version());

    RistrettoTable *table = ristretto_table_create("sensors",
        "CREATE TABLE sensors (name TEXT(32), reading REAL)");
    if (!table) {
        fprintf(stderr, "Failed to create table\n");
        return 1;
    }

    /* Two normal rows plus one with a NULL reading. */
    RistrettoValue r0[2] = { ristretto_value_text("temp_outside"), ristretto_value_real(21.5) };
    RistrettoValue r1[2] = { ristretto_value_text("humidity"),     ristretto_value_real(48.0) };
    RistrettoValue r2[2] = { ristretto_value_text("pressure"),     ristretto_value_null() };

    ristretto_table_append_row(table, r0);
    ristretto_table_append_row(table, r1);
    ristretto_table_append_row(table, r2);
    ristretto_value_destroy(&r0[0]);
    ristretto_value_destroy(&r1[0]);
    ristretto_value_destroy(&r2[0]);

    ristretto_table_flush_durable(table);
    printf("Wrote %zu rows (one with a NULL reading).\n\n", ristretto_table_get_row_count(table));

    printf("Scan:\n");
    ristretto_table_select(table, print_sensor, NULL);

    ristretto_table_close(table);
    printf("\nSUCCESS: Embedding demonstration completed.\n");
    return 0;
}
