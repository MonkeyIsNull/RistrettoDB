/*
** cbridge.c - C bridge implementation for the RistrettoDB Go bindings.
**
** cgo compiles every .c file in the package directory and links it in, so this
** file provides the small amount of glue the Go side needs: trampolines that
** forward to the Go callbacks exported via //export, plus accessors that read
** the Table V2 Value union and table schema without forcing cgo to model the
** C union itself.
*/
#include "cbridge.h"
#include "_cgo_export.h" /* goSelectCallback, goQueryCallback */

/* ---- Trampolines --------------------------------------------------------- */
int rdb_query(RistrettoDB *db, const char *sql, void *ctx) {
    return ristretto_query(db, sql, goQueryCallback, ctx);
}

int rdb_table_select(Table *t, void *ctx) {
    /* goSelectCallback is generated with a non-const row parameter; the C API
    ** takes const. The cast is safe: the callback only reads the row. */
    void (*cb)(void *, const Value *) = (void (*)(void *, const Value *))goSelectCallback;
    return table_select(t, NULL, cb, ctx) ? 0 : -1;
}

/* ---- Value accessors ----------------------------------------------------- */
int          rdbv_type(const Value *v)    { return (int)v->type; }
int          rdbv_is_null(const Value *v) { return v->is_null ? 1 : 0; }
long long    rdbv_int(const Value *v)     { return (long long)v->value.integer; }
double       rdbv_real(const Value *v)    { return v->value.real; }
const char  *rdbv_text(const Value *v)    { return v->value.text.data; }
const Value *rdbv_at(const Value *row, int i) { return &row[i]; }

/* ---- Schema introspection ------------------------------------------------ */
unsigned    rdb_col_count(Table *t)           { return t->header->column_count; }
int         rdb_col_type(Table *t, unsigned i) { return (int)t->header->columns[i].type; }
const char *rdb_col_name(Table *t, unsigned i) { return t->header->columns[i].name; }
