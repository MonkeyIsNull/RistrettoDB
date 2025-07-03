#ifndef RISTRETTO_DB_H
#define RISTRETTO_DB_H

#include <stdint.h>
#include <stddef.h>

typedef struct RistrettoDB RistrettoDB;

typedef enum {
    RISTRETTO_OK = 0,
    RISTRETTO_ERROR = -1,
    RISTRETTO_NOMEM = -2,
    RISTRETTO_IO_ERROR = -3,
    RISTRETTO_PARSE_ERROR = -4,
    RISTRETTO_NOT_FOUND = -5,
    RISTRETTO_CONSTRAINT_ERROR = -6
} RistrettoResult;

RistrettoDB* ristretto_open(const char* filename);
void ristretto_close(RistrettoDB* db);

RistrettoResult ristretto_exec(RistrettoDB* db, const char* sql);

typedef void (*RistrettoCallback)(void* ctx, int n_cols, char** values, char** col_names);
RistrettoResult ristretto_query(RistrettoDB* db, const char* sql, RistrettoCallback callback, void* ctx);

const char* ristretto_error_string(RistrettoResult result);

#endif
