/*
** RistrettoDB Working Embedding Example (Table V2)
**
** A compact, realistic telemetry example: append a batch of sensor readings
** and compute an aggregate during a single scan.
**
** To compile and run:
**   make static
**   clang -O3 -Iembed -o working_demo examples/working_demo.c lib/libristretto.a
**   ./working_demo
*/

#include "ristretto.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    long   rows;
    double cpu_sum;
} ScanState;

static void accumulate(void *ctx, const RistrettoValue *row) {
    ScanState *s = (ScanState *)ctx;
    s->rows++;
    if (!row[1].is_null) {
        s->cpu_sum += row[1].value.real;
    }
}

int main(void) {
    printf("=== RistrettoDB Working Demo (Table V2) ===\n");
    printf("Version: %s\n\n", ristretto_version());

    RistrettoTable *table = ristretto_table_create("host_metrics",
        "CREATE TABLE host_metrics (ts INTEGER, cpu_usage REAL, host TEXT(24))");
    if (!table) {
        fprintf(stderr, "Failed to create table\n");
        return 1;
    }

    const int N = 5000;
    printf("Appending %d readings...\n", N);
    for (int i = 0; i < N; i++) {
        RistrettoValue v[3];
        v[0] = ristretto_value_integer(1672531200 + i);
        v[1] = ristretto_value_real(10.0 + (i % 90));
        v[2] = ristretto_value_text("host-01");
        if (!ristretto_table_append_row(table, v)) {
            fprintf(stderr, "insert %d failed\n", i);
            break;
        }
        ristretto_value_destroy(&v[2]);
    }

    ScanState s = {0, 0.0};
    ristretto_table_select(table, accumulate, &s);
    printf("Scanned %ld rows; mean cpu_usage = %.2f%%\n",
           s.rows, s.rows ? s.cpu_sum / (double)s.rows : 0.0);

    ristretto_table_close(table);
    printf("\nSUCCESS: Working demo completed.\n");
    return 0;
}
