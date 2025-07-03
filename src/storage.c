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
    table->row_count = 0;
    table->next_row_id = 1;
    
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

// Page layout:
// [page_header: 8 bytes][row_slots: variable]
typedef struct {
    uint32_t page_type;      // 0 = data page
    uint32_t row_count;      // number of rows in this page
} PageHeader;

#define ROWS_PER_PAGE ((PAGE_SIZE - sizeof(PageHeader)) / sizeof(uint32_t))

RowId table_insert_row(Table *table, Pager *pager, Row *row) {
    // Simple implementation: always append to the last page
    if (table->root_page == 0) {
        table->root_page = pager_allocate_page(pager);
        void* page = pager_get_page(pager, table->root_page);
        PageHeader* header = (PageHeader*)page;
        header->page_type = 0;
        header->row_count = 0;
    }
    
    void* page = pager_get_page(pager, table->root_page);
    PageHeader* header = (PageHeader*)page;
    
    // Check if page has space
    if (header->row_count * table->row_size + sizeof(PageHeader) + table->row_size > PAGE_SIZE) {
        // TODO: Allocate new page or use B+tree
        return (RowId){0, 0}; // Out of space
    }
    
    // Copy row data to page
    uint8_t* row_data = (uint8_t*)page + sizeof(PageHeader) + header->row_count * table->row_size;
    memcpy(row_data, row->data, table->row_size);
    
    RowId row_id = {table->root_page, (uint16_t)(header->row_count * table->row_size + sizeof(PageHeader))};
    header->row_count++;
    table->row_count++;
    
    return row_id;
}

Row* table_get_row(Table *table, Pager *pager, RowId row_id) {
    void* page = pager_get_page(pager, row_id.page_id);
    if (!page) return NULL;
    
    uint8_t* row_data = (uint8_t*)page + row_id.offset;
    
    Row* row = malloc(sizeof(Row));
    if (!row) return NULL;
    
    row->size = table->row_size;
    row->data = malloc(table->row_size);
    if (!row->data) {
        free(row);
        return NULL;
    }
    
    memcpy(row->data, row_data, table->row_size);
    return row;
}


TableScanner* table_scanner_create(Table *table, Pager *pager) {
    TableScanner* scanner = malloc(sizeof(TableScanner));
    if (!scanner) return NULL;
    
    scanner->table = table;
    scanner->pager = pager;
    scanner->current_page = table->root_page;
    scanner->current_offset = sizeof(PageHeader);
    scanner->rows_scanned = 0;
    scanner->at_end = (table->row_count == 0);
    
    return scanner;
}

void table_scanner_destroy(TableScanner *scanner) {
    free(scanner);
}

Row* table_scanner_next(TableScanner *scanner) {
    if (scanner->at_end || scanner->rows_scanned >= scanner->table->row_count) {
        scanner->at_end = true;
        return NULL;
    }
    
    void* page = pager_get_page(scanner->pager, scanner->current_page);
    if (!page) {
        scanner->at_end = true;
        return NULL;
    }
    
    PageHeader* header = (PageHeader*)page;
    uint32_t row_index = (scanner->current_offset - sizeof(PageHeader)) / scanner->table->row_size;
    
    if (row_index >= header->row_count) {
        // TODO: Move to next page
        scanner->at_end = true;
        return NULL;
    }
    
    uint8_t* row_data = (uint8_t*)page + scanner->current_offset;
    
    Row* row = malloc(sizeof(Row));
    if (!row) return NULL;
    
    row->size = scanner->table->row_size;
    row->data = malloc(scanner->table->row_size);
    if (!row->data) {
        free(row);
        return NULL;
    }
    
    memcpy(row->data, row_data, scanner->table->row_size);
    
    // Advance to next row
    scanner->current_offset += scanner->table->row_size;
    scanner->rows_scanned++;
    
    return row;
}

bool table_scanner_at_end(TableScanner *scanner) {
    return scanner->at_end;
}
