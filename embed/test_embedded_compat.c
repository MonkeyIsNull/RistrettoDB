/*
** Smoke test: use RistrettoDB fully embedded (Option 1). Define
** RISTRETTO_EMBEDDED and #include the amalgamation .c directly — no separate
** compilation or linking required.
**
**   clang -O3 -Iembed -o test_embedded_compat embed/test_embedded_compat.c
**   ./test_embedded_compat
*/
#include <stdio.h>

#define RISTRETTO_EMBEDDED
#include "ristretto.c"

static void print_row(void *ctx, const RistrettoValue *row) {
    (void)ctx;
    printf("  id=%lld value=%.2f name=%s\n",
           (long long)row[0].value.integer,
           row[1].value.real,
           row[2].is_null ? "(null)" : row[2].value.text.data);
}

int main(void) {
    printf("Testing RistrettoDB Embedded (single-file, RISTRETTO_EMBEDDED)\n");
    printf("Version: %s\n\n", ristretto_version());

    RistrettoTable *table = ristretto_table_create("v2_embedded_test",
        "CREATE TABLE v2_embedded_test (id INTEGER, value REAL, name TEXT(32))");
    if (!table) {
        printf("ERROR: Failed to create table\n");
        return 1;
    }
    printf("SUCCESS: table created\n");

    for (int i = 0; i < 3; i++) {
        RistrettoValue v[3];
        v[0] = ristretto_value_integer(i + 1);
        v[1] = ristretto_value_real((i + 1) * 2.25);
        v[2] = ristretto_value_text("single-file");
        ristretto_table_append_row(table, v);
        ristretto_value_destroy(&v[2]);
    }
    printf("SUCCESS: appended %zu rows\n\n", ristretto_table_get_row_count(table));

    ristretto_table_select(table, print_row, NULL);
    ristretto_table_close(table);
    printf("\nSUCCESS: embedded single-file smoke test passed.\n");
    return 0;
}
