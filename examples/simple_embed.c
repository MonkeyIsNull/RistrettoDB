/*
** Simple RistrettoDB Embedding Example (Table V2, prefixed API)
**
** Shows how to embed RistrettoDB using the public ristretto_table_* /
** ristretto_value_* API from ristretto.h.
**
** To compile and run:
**   make static
**   clang -O3 -Iembed -o simple_embed examples/simple_embed.c lib/libristretto.a
**   ./simple_embed
*/

#include "ristretto.h"
#include <stdio.h>
#include <stdlib.h>

static void print_metric(void *ctx, const RistrettoValue *row) {
    long *count = (long *)ctx;
    if (*count < 3) {
        printf("  ts=%lld cpu=%.1f mem=%lld proc=%s\n",
               (long long)row[0].value.integer,
               row[1].value.real,
               (long long)row[2].value.integer,
               row[3].is_null ? "(null)" : row[3].value.text.data);
    }
    (*count)++;
}

int main(void) {
    printf("=== RistrettoDB Simple Embedding Example (Table V2) ===\n");
    printf("Version: %s\n\n", ristretto_version());

    RistrettoTable *table = ristretto_table_create("metrics",
        "CREATE TABLE metrics (timestamp INTEGER, cpu_usage REAL, memory_mb INTEGER, process TEXT(32))");
    if (!table) {
        fprintf(stderr, "Failed to create table\n");
        return 1;
    }

    printf("Inserting 1000 metric records...\n");
    for (int i = 0; i < 1000; i++) {
        RistrettoValue values[4];
        values[0] = ristretto_value_integer(1672531200 + i);
        values[1] = ristretto_value_real(15.5 + (i % 50));
        values[2] = ristretto_value_integer(512 + (i % 200));
        values[3] = ristretto_value_text("process_name");
        if (!ristretto_table_append_row(table, values)) {
            fprintf(stderr, "Failed to insert row %d\n", i);
            break;
        }
        ristretto_value_destroy(&values[3]);
    }

    printf("Total rows inserted: %zu\n\n", ristretto_table_get_row_count(table));

    printf("First rows scanned back:\n");
    long count = 0;
    ristretto_table_select(table, print_metric, &count);
    printf("Scanned %ld rows.\n", count);

    ristretto_table_close(table);
    printf("\nSUCCESS: Embedding example completed.\n");
    return 0;
}
