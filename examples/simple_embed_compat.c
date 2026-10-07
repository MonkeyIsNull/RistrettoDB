/*
** Simple RistrettoDB Embedding Example (compatibility layer)
**
** Identical to simple_embed.c, but uses the short, unprefixed names
** (Table, table_create, value_integer, ...) that ristretto.h maps onto the
** prefixed API via its compatibility layer. New code may prefer the prefixed
** names; this shows the compatibility aliases still work.
**
** To compile and run:
**   make static
**   clang -O3 -Iembed -o simple_embed_compat examples/simple_embed_compat.c lib/libristretto.a
**   ./simple_embed_compat
*/

#include "ristretto.h"
#include <stdio.h>
#include <stdlib.h>

static void print_metric(void *ctx, const Value *row) {
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
    printf("=== RistrettoDB Simple Embedding Example (Compatibility) ===\n");
    printf("Version: %s\n\n", ristretto_version());

    Table *table = table_create("metrics_compat",
        "CREATE TABLE metrics (timestamp INTEGER, cpu_usage REAL, memory_mb INTEGER, process TEXT(32))");
    if (!table) {
        fprintf(stderr, "Failed to create table\n");
        return 1;
    }

    printf("Inserting 1000 metric records...\n");
    for (int i = 0; i < 1000; i++) {
        Value values[4];
        values[0] = value_integer(1672531200 + i);
        values[1] = value_real(15.5 + (i % 50));
        values[2] = value_integer(512 + (i % 200));
        values[3] = value_text("process_name");
        if (!table_append_row(table, values)) {
            fprintf(stderr, "Failed to insert row %d\n", i);
            break;
        }
        value_destroy(&values[3]);
    }

    printf("Total rows inserted: %zu\n\n", table_get_row_count(table));

    printf("First rows scanned back:\n");
    long count = 0;
    table_select(table, print_metric, &count);
    printf("Scanned %ld rows.\n", count);

    table_close(table);
    printf("\nSUCCESS: Compatibility embedding example completed.\n");
    return 0;
}
