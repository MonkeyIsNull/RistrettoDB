/*
** cbridge.h - C bridge for the RistrettoDB Go bindings.
**
** This header declares exactly the symbols the Go package calls. The Table V2
** types and functions come from the project's public table_v2.h; the engine
** exports the unprefixed table_* / value_* symbols the Go side links against.
**
** We deliberately avoid embed/ristretto.h: its Table V2 prototypes use a
** "ristretto_table_*" prefix that does not match the "table_*" symbols the
** compiled library actually exports.
*/
#ifndef RISTRETTO_GO_CBRIDGE_H
#define RISTRETTO_GO_CBRIDGE_H

#include <stdint.h>
#include "table_v2.h"

/* ---- Version ------------------------------------------------------------- */
const char  *ristretto_version(void);
int          ristretto_version_number(void);

/* ---- Trampolines (keep Go function pointers off the boundary) ------------ */
int rdb_table_select(Table *t, void *ctx);

/* ---- Value accessors (keep the C union out of Go) ------------------------ */
int          rdbv_type(const Value *v);
int          rdbv_is_null(const Value *v);
long long    rdbv_int(const Value *v);
double       rdbv_real(const Value *v);
const char  *rdbv_text(const Value *v);
const Value *rdbv_at(const Value *row, int i);

/* ---- Schema introspection ------------------------------------------------ */
unsigned     rdb_col_count(Table *t);
int          rdb_col_type(Table *t, unsigned i);
const char  *rdb_col_name(Table *t, unsigned i);

#endif /* RISTRETTO_GO_CBRIDGE_H */
