#ifndef RISTRETTO_STORAGE_H
#define RISTRETTO_STORAGE_H

#include <stdint.h>
#include <stddef.h>
#include "pager.h"

// Forward declaration for BTree
struct BTree;

typedef enum {
    TYPE_NULL = 0,
    TYPE_INTEGER = 1,
    TYPE_REAL = 2,
    TYPE_TEXT = 3
} DataType;

typedef struct {
    DataType type;
    union {
        int64_t integer;
        double real;
        struct {
            char *data;
            size_t len;
        } text;
    } value;
} SqlValue;

typedef struct {
    char name[32];
    DataType type;
    size_t offset;
    size_t size;
} Column;

typedef struct {
    char name[64];
    uint32_t column_count;
    Column *columns;
    size_t row_size;
    uint32_t root_page;
    uint32_t row_count;
    uint32_t next_row_id;
    struct BTree *primary_index; // B-tree index on first INTEGER column (if exists)
} SqlTable;

typedef struct {
    uint32_t page_id;
    uint16_t offset;
} RowId;

typedef struct {
    uint8_t *data;
    size_t size;
} Row;

SqlTable* storage_table_create(const char *name);
void storage_table_destroy(SqlTable *table);

void storage_table_add_column(SqlTable *table, const char *name, DataType type);

Row* storage_row_create(SqlTable *table);
void storage_row_destroy(Row *row);

void storage_row_set_value(Row *row, SqlTable *table, uint32_t col_index, SqlValue *value);
SqlValue* storage_row_get_value(Row *row, SqlTable *table, uint32_t col_index);
void storage_value_destroy(SqlValue *value);

// SqlTable storage operations
RowId table_insert_row(SqlTable *table, Pager *pager, Row *row);
Row* table_get_row(SqlTable *table, Pager *pager, RowId row_id);

// SqlTable scanning
typedef struct {
    SqlTable *table;
    Pager *pager;
    uint32_t current_page;
    uint32_t current_offset;
    uint32_t rows_scanned;
    bool at_end;
} TableScanner;

TableScanner* table_scanner_create(SqlTable *table, Pager *pager);
void table_scanner_destroy(TableScanner *scanner);
Row* table_scanner_next(TableScanner *scanner);
bool table_scanner_at_end(TableScanner *scanner);

#endif
