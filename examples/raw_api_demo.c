/*
** RistrettoDB Raw API Demo (Table V2)
**
** Demonstrates calling the exact unprefixed table_* / value_* symbols exported
** by libristretto using local `extern` declarations, without including any
** RistrettoDB header. This is the lowest-level way to embed the engine.
**
** To compile and run:
**   make static                 # Build lib/libristretto.a first
**   clang -O3 -o raw_api_demo examples/raw_api_demo.c lib/libristretto.a
**   ./raw_api_demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/* Opaque table handle. */
typedef struct Table Table;

/* Value layout mirrors include/table_v2.h. */
typedef enum {
    COL_TYPE_INTEGER = 1,
    COL_TYPE_REAL = 2,
    COL_TYPE_TEXT = 3,
    COL_TYPE_NULLABLE = 4
} ColumnType;

typedef struct {
    ColumnType type;
    union {
        int64_t integer;
        double real;
        struct {
            char *data;
            size_t length;
        } text;
    } value;
    bool is_null;
} Value;

/* Exported engine symbols (unprefixed). */
extern const char* ristretto_version(void);
extern Table* table_create_ex(const char *name, const char *schema_sql,
                              const char *base_dir, int open_mode);
extern void table_close(Table *table);
extern bool table_append_row(Table *table, const Value *values);
extern bool table_select(Table *table,
                         void (*callback)(void *ctx, const Value *row), void *ctx);
extern size_t table_get_row_count(Table *table);

extern Value value_integer(int64_t val);
extern Value value_text(const char *str);
extern void value_destroy(Value *value);

static void print_event(void *ctx, const Value *row) {
    int *shown = (int *)ctx;
    if (*shown >= 3) return;   /* only print the first few */
    printf("  event_id=%lld severity=%lld message=\"%s\"\n",
           (long long)row[0].value.integer,
           (long long)row[1].value.integer,
           row[2].is_null ? "(null)" : row[2].value.text.data);
    (*shown)++;
}

int main(void) {
    printf("==============================================\n");
    printf("    RistrettoDB Raw API Demo (Table V2)\n");
    printf("==============================================\n");
    printf("Library Version: %s\n\n", ristretto_version());

    Table *table = table_create_ex("events",
        "CREATE TABLE events (event_id INTEGER, severity INTEGER, message TEXT(64))",
        "data", /* RDB_CREATE_OR_TRUNCATE */ 1);
    if (!table) {
        fprintf(stderr, "ERROR: Failed to create table\n");
        return 1;
    }
    printf("SUCCESS: table 'events' created\n");

    const char *event_types[] = {
        "INFO: System startup",
        "WARN: Memory usage high",
        "ERROR: Connection failed",
        "DEBUG: Processing request",
        "FATAL: System crash"
    };

    int inserted = 0;
    for (int i = 0; i < 3000; i++) {
        Value values[3];
        values[0] = value_integer(1000 + i);
        values[1] = value_integer(i % 5);
        values[2] = value_text(event_types[i % 5]);
        if (table_append_row(table, values)) inserted++;
        value_destroy(&values[2]);
    }

    printf("SUCCESS: logged %d events\n", inserted);
    printf("   Total events in table: %zu\n\n", table_get_row_count(table));

    printf("First rows scanned back:\n");
    int shown = 0;
    table_select(table, print_event, &shown);

    table_close(table);
    printf("\nSUCCESS: Raw API demo completed.\n");
    return 0;
}
