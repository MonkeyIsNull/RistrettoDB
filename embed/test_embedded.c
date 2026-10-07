/*
** Smoke test: use RistrettoDB via the public header + separately-compiled
** amalgamation (Option 2). Compile embed/ristretto.c as its own unit (with
** -DRISTRETTO_EMBEDDED so it supplies the full implementation) and link.
**
**   clang -O3 -Iembed -DRISTRETTO_EMBEDDED -c embed/ristretto.c -o ristretto.o
**   clang -O3 -Iembed -o test_embedded embed/test_embedded.c ristretto.o
**   ./test_embedded
*/
#include "ristretto.h"
#include <stdio.h>

static void print_row(void *ctx, const RistrettoValue *row) {
    (void)ctx;
    printf("  id=%lld value=%.2f name=%s\n",
           (long long)row[0].value.integer,
           row[1].value.real,
           row[2].is_null ? "(null)" : row[2].value.text.data);
}

int main(void) {
    printf("Testing RistrettoDB Embedded (linked amalgamation)\n");
    printf("Version: %s\n\n", ristretto_version());

    RistrettoTable *table = ristretto_table_create("v2_test",
        "CREATE TABLE v2_test (id INTEGER, value REAL, name TEXT(32))");
    if (!table) {
        printf("ERROR: Failed to create table\n");
        return 1;
    }
    printf("SUCCESS: table created\n");

    for (int i = 0; i < 3; i++) {
        RistrettoValue v[3];
        v[0] = ristretto_value_integer(i + 1);
        v[1] = ristretto_value_real((i + 1) * 1.5);
        v[2] = ristretto_value_text("embedded");
        ristretto_table_append_row(table, v);
        ristretto_value_destroy(&v[2]);
    }
    printf("SUCCESS: appended %zu rows\n\n", ristretto_table_get_row_count(table));

    ristretto_table_select(table, print_row, NULL);
    ristretto_table_close(table);
    printf("\nSUCCESS: embedded smoke test passed.\n");
    return 0;
}
