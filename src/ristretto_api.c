/*
** ristretto_api.c - Exported public Table V2 API (ristretto_* prefix).
**
** The library's internal Table V2 engine (src/table_v2.c) exports the
** unprefixed table_* / value_* symbols that the Go binding and the C test
** suites link against. The public embedding header (embed/ristretto.h)
** presents the same engine under the ristretto_* prefix. This file provides
** the thin exported wrappers that forward the prefixed public API to those
** internal symbols, so an application that includes only ristretto.h and links
** libristretto gets a real, link-able Table V2 surface.
**
** Ordering is load-bearing: RISTRETTO_NO_COMPATIBILITY_LAYER must be defined
** BEFORE including ristretto.h, otherwise the header's compatibility macros
** (#define table_create ristretto_table_create, ...) would rewrite the wrapper
** bodies below into infinite self-recursion.
*/
#define RISTRETTO_NO_COMPATIBILITY_LAYER
#include "ristretto.h"   /* public prefixed types: RistrettoValue, ...        */
#include "table_v2.h"    /* internal engine types: Value, Table, ColumnDesc   */

#include <string.h>

/*
** The public RistrettoValue / RistrettoColumnDesc / RistrettoTableHeader types
** are layout-identical to the internal Value / ColumnDesc / TableHeader types
** (same field order, same MAX_COLUMN_NAME, same header size). These tripwires
** prove that per build platform; the ColumnDesc assert in particular would
** have caught the historical 8-vs-32 MAX_COLUMN_NAME ABI mismatch at compile
** time.
*/
_Static_assert(sizeof(RistrettoValue) == sizeof(Value),
               "RistrettoValue / Value ABI mismatch");
_Static_assert(sizeof(RistrettoColumnDesc) == sizeof(ColumnDesc),
               "RistrettoColumnDesc / ColumnDesc ABI mismatch");
_Static_assert(sizeof(RistrettoTableHeader) == sizeof(TableHeader),
               "RistrettoTableHeader / TableHeader ABI mismatch");

/* C treats RistrettoValue and Value as distinct types even though they share a
** layout, so a by-value Value cannot be returned directly as a RistrettoValue.
** Convert explicitly through a byte copy. */
static RistrettoValue rv_from_value(Value v) {
    RistrettoValue r;
    memcpy(&r, &v, sizeof r);
    return r;
}

/* ---- Table lifecycle ----------------------------------------------------- */
RistrettoTable* ristretto_table_create(const char *name, const char *schema_sql) {
    return (RistrettoTable*)table_create(name, schema_sql);
}

RistrettoTable* ristretto_table_open(const char *name) {
    return (RistrettoTable*)table_open(name);
}

RistrettoTable* ristretto_table_create_ex(const char *name, const char *schema_sql,
                                          const char *base_dir, int open_mode) {
    return (RistrettoTable*)table_create_ex(name, schema_sql, base_dir, open_mode);
}

RistrettoTable* ristretto_table_open_ex(const char *name, const char *base_dir) {
    return (RistrettoTable*)table_open_ex(name, base_dir);
}

void ristretto_table_close(RistrettoTable *table) {
    table_close((Table*)table);
}

/* ---- Core operations ----------------------------------------------------- */
bool ristretto_table_append_row(RistrettoTable *table, const RistrettoValue *values) {
    return table_append_row((Table*)table, (const Value*)values);
}

bool ristretto_table_append_row_n(RistrettoTable *table, const RistrettoValue *values,
                                  uint32_t value_count) {
    return table_append_row_n((Table*)table, (const Value*)values, value_count);
}

/* The internal engine invokes the scan callback through a pointer of type
** void(*)(void *, const Value *). Casting the public
** void(*)(void *, const RistrettoValue *) to that type and calling through it
** is undefined behavior that UBSan's -fsanitize=function rightly flags
** ("call through pointer to incorrect function type"), even though the two row
** types are layout-identical. Forward through a trampoline whose signature
** EXACTLY matches the internal callback type instead; converting the (compatible)
** row object pointer inside it is well-defined. */
typedef struct {
    void (*user_cb)(void *ctx, const RistrettoValue *row);
    void *user_ctx;
} RistrettoSelectForward;

static void ristretto_select_trampoline(void *ctx, const Value *row) {
    const RistrettoSelectForward *fwd = (const RistrettoSelectForward *)ctx;
    fwd->user_cb(fwd->user_ctx, (const RistrettoValue *)row);
}

bool ristretto_table_select(RistrettoTable *table,
                            void (*callback)(void *ctx, const RistrettoValue *row),
                            void *ctx) {
    if (!callback) return false;
    RistrettoSelectForward fwd = { callback, ctx };
    return table_select((Table*)table, ristretto_select_trampoline, &fwd);
}

/* ---- File management ----------------------------------------------------- */
bool ristretto_table_flush(RistrettoTable *table) {
    return table_flush((Table*)table);
}

bool ristretto_table_flush_durable(RistrettoTable *table) {
    return table_flush_durable((Table*)table);
}

bool ristretto_table_remap(RistrettoTable *table) {
    return table_remap((Table*)table);
}

bool ristretto_table_ensure_space(RistrettoTable *table, size_t needed_bytes) {
    return table_ensure_space((Table*)table, needed_bytes);
}

/* ---- Schema and metadata ------------------------------------------------- */
bool ristretto_table_parse_schema(const char *schema_sql, RistrettoColumnDesc *columns,
                                  uint32_t *column_count, uint32_t *row_size) {
    return table_parse_schema(schema_sql, (ColumnDesc*)columns, column_count, row_size);
}

const RistrettoColumnDesc* ristretto_table_get_column(RistrettoTable *table, const char *name) {
    return (const RistrettoColumnDesc*)table_get_column((Table*)table, name);
}

size_t ristretto_table_get_row_count(RistrettoTable *table) {
    return table_get_row_count((Table*)table);
}

/* ---- Row packing/unpacking ----------------------------------------------- */
bool ristretto_table_pack_row(RistrettoTable *table, const RistrettoValue *values,
                              uint8_t *row_buffer) {
    return table_pack_row((Table*)table, (const Value*)values, row_buffer);
}

bool ristretto_table_unpack_row(RistrettoTable *table, const uint8_t *row_buffer,
                                RistrettoValue *values) {
    return table_unpack_row((Table*)table, row_buffer, (Value*)values);
}

/* ---- Value utilities ----------------------------------------------------- */
RistrettoValue ristretto_value_integer(int64_t val) { return rv_from_value(value_integer(val)); }
RistrettoValue ristretto_value_real(double val)     { return rv_from_value(value_real(val)); }
RistrettoValue ristretto_value_text(const char *str){ return rv_from_value(value_text(str)); }
RistrettoValue ristretto_value_null(void)           { return rv_from_value(value_null()); }

void ristretto_value_destroy(RistrettoValue *value) {
    value_destroy((Value*)value);
}

/* ---- Utility functions --------------------------------------------------- */
uint64_t ristretto_get_time_ms(void)         { return get_time_ms(); }
bool     ristretto_create_data_directory(void) { return create_data_directory(); }
