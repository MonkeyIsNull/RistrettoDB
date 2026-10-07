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
#include "_cgo_export.h" /* goSelectCallback */

/* ---- Trampolines --------------------------------------------------------- */
/* table_select's callback type is void(*)(void *, const Value *). cgo generates
** goSelectCallback with a non-const row parameter, so its type is
** void(*)(void *, Value *). Casting one function pointer to the other and
** calling through it is undefined behavior (-fsanitize=function flags it as a
** "call through pointer to incorrect function type").
**
** Instead we forward through this trampoline, whose signature matches the
** table_select callback type EXACTLY, so table_select always calls through the
** correct function type. Inside, we convert only the row OBJECT pointer: the
** const cast is well-defined because goSelectCallback only reads the row. */
static void rdb_select_trampoline(void *ctx, const Value *row) {
    goSelectCallback(ctx, (Value *)row);
}

int rdb_table_select(Table *t, void *ctx) {
    return table_select(t, rdb_select_trampoline, ctx) ? 0 : -1;
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
