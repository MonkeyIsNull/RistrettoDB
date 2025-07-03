#include "storage.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define ALIGN_SIZE 8
#define MAX_TEXT_SIZE 255

static size_t get_type_size(DataType type) {
    switch (type) {
        case TYPE_NULL:
            return 0;
        case TYPE_INTEGER:
            return sizeof(int64_t);
        case TYPE_REAL:
            return sizeof(double);
        case TYPE_TEXT:
            return MAX_TEXT_SIZE + 1;
        default:
            return 0;
    }
}

static size_t align_offset(size_t offset) {
    return (offset + ALIGN_SIZE - 1) & ~(ALIGN_SIZE - 1);
}

Table* table_create(const char* name) {
    Table* table = malloc(sizeof(Table));
    if (!table) {
        return NULL;
    }
    
    strncpy(table->name, name, sizeof(table->name) - 1);
    table->name[sizeof(table->name) - 1] = '\0';
    
    table->column_count = 0;
    table->columns = NULL;
    table->row_size = 0;
    table->root_page = 0;
    
    return table;
}

void table_destroy(Table* table) {
    if (!table) {
        return;
    }
    
    free(table->columns);
    free(table);
}

void table_add_column(Table* table, const char* name, DataType type) {
    uint32_t new_count = table->column_count + 1;
    Column* new_columns = realloc(table->columns, new_count * sizeof(Column));
    if (!new_columns) {
        return;
    }
    
    table->columns = new_columns;
    Column* col = &table->columns[table->column_count];
    
    strncpy(col->name, name, sizeof(col->name) - 1);
    col->name[sizeof(col->name) - 1] = '\0';
    
    col->type = type;
    col->size = get_type_size(type);
    col->offset = align_offset(table->row_size);
    
    table->row_size = col->offset + col->size;
    table->column_count = new_count;
}

Row* row_create(Table* table) {
    Row* row = malloc(sizeof(Row));
    if (!row) {
        return NULL;
    }
    
    row->size = table->row_size;
    row->data = calloc(1, row->size);
    if (!row->data) {
        free(row);
        return NULL;
    }
    
    return row;
}

void row_destroy(Row* row) {
    if (!row) {
        return;
    }
    
    free(row->data);
    free(row);
}

void row_set_value(Row* row, Table* table, uint32_t col_index, Value* value) {
    if (col_index >= table->column_count) {
        return;
    }
    
    Column* col = &table->columns[col_index];
    uint8_t* dest = row->data + col->offset;
    
    switch (col->type) {
        case TYPE_NULL:
            break;
            
        case TYPE_INTEGER:
            if (value->type == TYPE_INTEGER) {
                memcpy(dest, &value->value.integer, sizeof(int64_t));
            }
            break;
            
        case TYPE_REAL:
            if (value->type == TYPE_REAL) {
                memcpy(dest, &value->value.real, sizeof(double));
            }
            break;
            
        case TYPE_TEXT:
            if (value->type == TYPE_TEXT && value->value.text.data) {
                size_t copy_len = value->value.text.len;
                if (copy_len > MAX_TEXT_SIZE) {
                    copy_len = MAX_TEXT_SIZE;
                }
                memcpy(dest, value->value.text.data, copy_len);
                dest[copy_len] = '\0';
            }
            break;
    }
}

Value* row_get_value(Row* row, Table* table, uint32_t col_index) {
    if (col_index >= table->column_count) {
        return NULL;
    }
    
    Value* value = malloc(sizeof(Value));
    if (!value) {
        return NULL;
    }
    
    Column* col = &table->columns[col_index];
    uint8_t* src = row->data + col->offset;
    
    value->type = col->type;
    
    switch (col->type) {
        case TYPE_NULL:
            break;
            
        case TYPE_INTEGER:
            memcpy(&value->value.integer, src, sizeof(int64_t));
            break;
            
        case TYPE_REAL:
            memcpy(&value->value.real, src, sizeof(double));
            break;
            
        case TYPE_TEXT:
            value->value.text.len = strlen((char*)src);
            value->value.text.data = malloc(value->value.text.len + 1);
            if (value->value.text.data) {
                strcpy(value->value.text.data, (char*)src);
            }
            break;
    }
    
    return value;
}

void value_destroy(Value* value) {
    if (!value) {
        return;
    }
    
    if (value->type == TYPE_TEXT && value->value.text.data) {
        free(value->value.text.data);
    }
    
    free(value);
}
