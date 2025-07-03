#ifndef RISTRETTO_STORAGE_H
#define RISTRETTO_STORAGE_H

#include <stdint.h>
#include <stddef.h>

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
} Table;

typedef struct {
    uint32_t page_id;
    uint16_t offset;
} RowId;

typedef struct {
    uint8_t *data;
    size_t size;
} Row;

Table* table_create(const char *name);
void table_destroy(Table *table);

void table_add_column(Table *table, const char *name, DataType type);

Row* row_create(Table *table);
void row_destroy(Row *row);

void row_set_value(Row *row, Table *table, uint32_t col_index, Value *value);
Value* row_get_value(Row *row, Table *table, uint32_t col_index);

#endif
