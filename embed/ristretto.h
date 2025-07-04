/*
** RistrettoDB - A tiny, blazingly fast, embeddable SQL engine
** 
** This file contains the complete public API for RistrettoDB.
** Simply include this header and link against libristretto.a or libristretto.so
** 
** RistrettoDB provides two complementary APIs:
** 1. Original SQL API - General-purpose SQL with 2.8x SQLite performance
** 2. Table V2 API - Ultra-fast append-only tables with 4.57x SQLite performance
**
** Licensed under the MIT License.
** 
** To use RistrettoDB in your application:
**   #include "ristretto.h"
**   // Use RISTRETTO_SQL_API or RISTRETTO_V2_API functions
*/

#ifndef RISTRETTO_H
#define RISTRETTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
** RistrettoDB Version Information
*/
#define RISTRETTO_VERSION        "2.0.0"
#define RISTRETTO_VERSION_NUMBER 2000000
#define RISTRETTO_VERSION_MAJOR  2
#define RISTRETTO_VERSION_MINOR  0
#define RISTRETTO_VERSION_PATCH  0

/*
** Return the version string
*/
const char* ristretto_version(void);

/*
** Return the version number
*/
int ristretto_version_number(void);

/*
** =============================================================================
** ORIGINAL SQL API - General-purpose SQL with 2.8x SQLite performance
** =============================================================================
*/

/*
** Database handle - opaque structure
*/
typedef struct RistrettoDB RistrettoDB;

/*
** Result codes
*/
typedef enum {
    RISTRETTO_OK = 0,
    RISTRETTO_ERROR = -1,
    RISTRETTO_NOMEM = -2,
    RISTRETTO_IO_ERROR = -3,
    RISTRETTO_PARSE_ERROR = -4,
    RISTRETTO_NOT_FOUND = -5,
    RISTRETTO_CONSTRAINT_ERROR = -6
} RistrettoResult;

/*
** Database lifecycle functions
*/
RistrettoDB* ristretto_open(const char* filename);
void ristretto_close(RistrettoDB* db);

/*
** Execute SQL statement (DDL/DML)
*/
RistrettoResult ristretto_exec(RistrettoDB* db, const char* sql);

/*
** Query callback function type
*/
typedef void (*RistrettoCallback)(void* ctx, int n_cols, char** values, char** col_names);

/*
** Execute SQL query with callback
*/
RistrettoResult ristretto_query(RistrettoDB* db, const char* sql, RistrettoCallback callback, void* ctx);

/*
** Get error string for result code
*/
const char* ristretto_error_string(RistrettoResult result);

/*
** =============================================================================
** TABLE V2 API - Ultra-fast append-only tables with 4.57x SQLite performance
** =============================================================================
*/

/*
** Constants for Table V2 API
*/
#define RISTRETTO_MAX_COLUMNS 14
#define RISTRETTO_MAX_COLUMN_NAME 8
#define RISTRETTO_TABLE_HEADER_SIZE 256
#define RISTRETTO_INITIAL_FILE_SIZE (1024 * 1024)  // 1 MB initial size
#define RISTRETTO_GROWTH_FACTOR 2                   // Double size when growing
#define RISTRETTO_SYNC_INTERVAL_ROWS 512           // Sync every N rows
#define RISTRETTO_SYNC_INTERVAL_MS 100             // Sync every N milliseconds

/*
** Magic bytes for file format identification
*/
#define RISTRETTO_TABLE_MAGIC "RSTRDB\x00\x00"
#define RISTRETTO_TABLE_VERSION 1

/*
** Column data types
*/
typedef enum {
    RISTRETTO_COL_INTEGER = 1,
    RISTRETTO_COL_REAL = 2,
    RISTRETTO_COL_TEXT = 3,
    RISTRETTO_COL_NULLABLE = 4
} RistrettoColumnType;

/*
** Column descriptor
*/
typedef struct {
    char name[RISTRETTO_MAX_COLUMN_NAME];  // Column name (truncated/padded)
    uint8_t type;                          // RistrettoColumnType
    uint8_t length;                        // Bytes if TEXT, 0 for INTEGER/REAL
    uint16_t offset;                       // Byte offset within row
    uint8_t reserved[4];                   // Padding/reserved for future use
} RistrettoColumnDesc;

/*
** Table header structure
*/
typedef struct {
    char magic[8];               // "RSTRDB\x00\x00"
    uint32_t version;            // File format version
    uint32_t row_size;           // Size in bytes of a single row
    uint64_t num_rows;           // Number of rows written
    uint32_t column_count;       // Number of columns
    uint8_t reserved[12];        // Reserved for future use
    RistrettoColumnDesc columns[RISTRETTO_MAX_COLUMNS];  // Column descriptors
} RistrettoTableHeader;

/*
** Table handle - opaque structure
*/
typedef struct RistrettoTable RistrettoTable;

/*
** Value structure for Table V2 API
*/
typedef struct {
    RistrettoColumnType type;
    union {
        int64_t integer;
        double real;
        struct {
            char *data;
            size_t length;
        } text;
    } value;
    bool is_null;
} RistrettoValue;

/*
** Table lifecycle functions
*/
RistrettoTable* ristretto_table_create(const char *name, const char *schema_sql);
RistrettoTable* ristretto_table_open(const char *name);
void ristretto_table_close(RistrettoTable *table);

/*
** Core table operations
*/
bool ristretto_table_append_row(RistrettoTable *table, const RistrettoValue *values);
bool ristretto_table_select(RistrettoTable *table, const char *where_clause,
                           void (*callback)(void *ctx, const RistrettoValue *row), void *ctx);

/*
** File management
*/
bool ristretto_table_flush(RistrettoTable *table);
bool ristretto_table_remap(RistrettoTable *table);
bool ristretto_table_ensure_space(RistrettoTable *table, size_t needed_bytes);

/*
** Schema and metadata
*/
bool ristretto_table_parse_schema(const char *schema_sql, RistrettoColumnDesc *columns,
                                 uint32_t *column_count, uint32_t *row_size);
const RistrettoColumnDesc* ristretto_table_get_column(RistrettoTable *table, const char *name);
size_t ristretto_table_get_row_count(RistrettoTable *table);

/*
** Value utilities
*/
RistrettoValue ristretto_value_integer(int64_t val);
RistrettoValue ristretto_value_real(double val);
RistrettoValue ristretto_value_text(const char *str);
RistrettoValue ristretto_value_null(void);
void ristretto_value_destroy(RistrettoValue *value);

/*
** Row packing/unpacking
*/
bool ristretto_table_pack_row(RistrettoTable *table, const RistrettoValue *values, uint8_t *row_buffer);
bool ristretto_table_unpack_row(RistrettoTable *table, const uint8_t *row_buffer, RistrettoValue *values);

/*
** Utility functions
*/
uint64_t ristretto_get_time_ms(void);
bool ristretto_create_data_directory(void);

/*
** =============================================================================
** COMPATIBILITY LAYER
** =============================================================================
** 
** For backward compatibility, the original function names are preserved
** as macros. New code should use the prefixed versions above.
*/

#ifndef RISTRETTO_NO_COMPATIBILITY_LAYER

/* Original SQL API compatibility */
#define RistrettoDB                  RistrettoDB
#define RistrettoResult              RistrettoResult
#define RistrettoCallback            RistrettoCallback
#define ristretto_open               ristretto_open
#define ristretto_close              ristretto_close
#define ristretto_exec               ristretto_exec
#define ristretto_query              ristretto_query
#define ristretto_error_string       ristretto_error_string

/* Table V2 API compatibility */
#define Table                        RistrettoTable
#define Value                        RistrettoValue
#define ColumnDesc                   RistrettoColumnDesc
#define ColumnType                   RistrettoColumnType
#define TableHeader                  RistrettoTableHeader
#define COL_TYPE_INTEGER             RISTRETTO_COL_INTEGER
#define COL_TYPE_REAL                RISTRETTO_COL_REAL
#define COL_TYPE_TEXT                RISTRETTO_COL_TEXT
#define COL_TYPE_NULLABLE            RISTRETTO_COL_NULLABLE
#define MAX_COLUMNS                  RISTRETTO_MAX_COLUMNS
#define MAX_COLUMN_NAME              RISTRETTO_MAX_COLUMN_NAME
#define TABLE_HEADER_SIZE            RISTRETTO_TABLE_HEADER_SIZE
#define INITIAL_FILE_SIZE            RISTRETTO_INITIAL_FILE_SIZE
#define GROWTH_FACTOR                RISTRETTO_GROWTH_FACTOR
#define SYNC_INTERVAL_ROWS           RISTRETTO_SYNC_INTERVAL_ROWS
#define SYNC_INTERVAL_MS             RISTRETTO_SYNC_INTERVAL_MS
#define TABLE_MAGIC                  RISTRETTO_TABLE_MAGIC
#define TABLE_VERSION                RISTRETTO_TABLE_VERSION

#define table_create                 ristretto_table_create
#define table_open                   ristretto_table_open
#define table_close                  ristretto_table_close
#define table_append_row             ristretto_table_append_row
#define table_select                 ristretto_table_select
#define table_flush                  ristretto_table_flush
#define table_remap                  ristretto_table_remap
#define table_ensure_space           ristretto_table_ensure_space
#define table_parse_schema           ristretto_table_parse_schema
#define table_get_column             ristretto_table_get_column
#define table_get_row_count          ristretto_table_get_row_count
#define value_integer                ristretto_value_integer
#define value_real                   ristretto_value_real
#define value_text                   ristretto_value_text
#define value_null                   ristretto_value_null
#define value_destroy                ristretto_value_destroy
#define table_pack_row               ristretto_table_pack_row
#define table_unpack_row             ristretto_table_unpack_row
#define get_time_ms                  ristretto_get_time_ms
#define create_data_directory        ristretto_create_data_directory

#endif /* RISTRETTO_NO_COMPATIBILITY_LAYER */

#ifdef __cplusplus
}
#endif

#endif /* RISTRETTO_H */