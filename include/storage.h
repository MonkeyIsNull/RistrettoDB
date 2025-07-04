#ifndef RISTRETTO_STORAGE_H
#define RISTRETTO_STORAGE_H

#include <stdint.h>
#include <stddef.h>
#include "pager.h"

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
} Value;

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
} Table;

typedef struct {
    uint32_t page_id;
    uint16_t offset;
} RowId;

typedef struct {
    uint8_t *data;
    size_t size;
} Row;

Table* storage_table_create(const char *name);
void storage_table_destroy(Table *table);

void storage_table_add_column(Table *table, const char *name, DataType type);

Row* storage_row_create(Table *table);
void storage_row_destroy(Row *row);

void storage_row_set_value(Row *row, Table *table, uint32_t col_index, Value *value);
Value* storage_row_get_value(Row *row, Table *table, uint32_t col_index);
void storage_value_destroy(Value *value);

// Table storage operations
RowId table_insert_row(Table *table, Pager *pager, Row *row);
Row* table_get_row(Table *table, Pager *pager, RowId row_id);

// Table scanning
typedef struct {
    Table *table;
    Pager *pager;
    uint32_t current_page;
    uint32_t current_offset;
    uint32_t rows_scanned;
    bool at_end;
} TableScanner;

TableScanner* table_scanner_create(Table *table, Pager *pager);
void table_scanner_destroy(TableScanner *scanner);
Row* table_scanner_next(TableScanner *scanner);
bool table_scanner_at_end(TableScanner *scanner);

#endif
