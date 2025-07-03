#include "query.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char name[64];
    Table* table;
} TableEntry;

typedef struct {
    TableEntry* entries;
    uint32_t count;
    uint32_t capacity;
} TableCatalog;

static TableCatalog* get_catalog(RistrettoDB* db) {
    // For now, use a global catalog per process
    // In a real implementation, this would be stored in the database file
    static TableCatalog catalog = {NULL, 0, 0};
    (void)db; // Suppress unused parameter warning
    return &catalog;
}

static Table* find_table(RistrettoDB* db, const char* name) {
    TableCatalog* catalog = get_catalog(db);
    
    for (uint32_t i = 0; i < catalog->count; i++) {
        if (strcmp(catalog->entries[i].name, name) == 0) {
            return catalog->entries[i].table;
        }
    }
    
    return NULL;
}

static bool register_table(RistrettoDB* db, Table* table) {
    TableCatalog* catalog = get_catalog(db);
    
    if (catalog->count >= catalog->capacity) {
        uint32_t new_cap = catalog->capacity ? catalog->capacity * 2 : 4;
        TableEntry* new_entries = realloc(catalog->entries, new_cap * sizeof(TableEntry));
        if (!new_entries) return false;
        catalog->entries = new_entries;
        catalog->capacity = new_cap;
    }
    
    strncpy(catalog->entries[catalog->count].name, table->name, 63);
    catalog->entries[catalog->count].name[63] = '\0';
    catalog->entries[catalog->count].table = table;
    catalog->count++;
    
    return true;
}

QueryPlan* plan_statement(Statement* stmt, RistrettoDB* db) {
    QueryPlan* plan = malloc(sizeof(QueryPlan));
    if (!plan) return NULL;
    
    switch (stmt->type) {
        case STMT_CREATE_TABLE:
            plan->type = PLAN_CREATE_TABLE;
            plan->data.create_table.stmt = &stmt->data.create_table;
            plan->table = NULL;
            break;
            
        case STMT_INSERT:
            plan->type = PLAN_INSERT;
            plan->table = find_table(db, stmt->data.insert.table_name);
            if (!plan->table) {
                free(plan);
                return NULL;
            }
            plan->data.insert.values = stmt->data.insert.values;
            plan->data.insert.value_count = stmt->data.insert.value_count;
            break;
            
        case STMT_SELECT:
            plan->type = PLAN_TABLE_SCAN;
            plan->table = find_table(db, stmt->data.select.table_name);
            if (!plan->table) {
                free(plan);
                return NULL;
            }
            plan->data.scan.filter = stmt->data.select.where_clause;
            plan->data.scan.columns = NULL;
            plan->data.scan.column_count = 0;
            break;
            
        default:
            free(plan);
            return NULL;
    }
    
    return plan;
}

void plan_destroy(QueryPlan* plan) {
    if (!plan) {
        return;
    }
    
    switch (plan->type) {
        case PLAN_TABLE_SCAN:
        case PLAN_INDEX_SCAN:
            free(plan->data.scan.columns);
            // Filter is owned by the statement, not the plan
            break;
            
        case PLAN_INSERT:
            // Values are owned by the statement, not the plan
            break;
            
        case PLAN_UPDATE:
            free(plan->data.update.columns);
            // Values and filter are owned by the statement, not the plan
            break;
            
        case PLAN_DELETE:
            // Filter is owned by the statement, not the plan
            break;
            
        case PLAN_CREATE_TABLE:
            // CreateTableStmt is owned by the statement, not the plan
            break;
    }
    
    free(plan);
}

static RistrettoResult execute_create_table(QueryContext* ctx) {
    CreateTableStmt* stmt = ctx->plan->data.create_table.stmt;
    
    // Check if table already exists
    if (find_table(ctx->db, stmt->table_name)) {
        return RISTRETTO_CONSTRAINT_ERROR;
    }
    
    // Create new table
    Table* table = table_create(stmt->table_name);
    if (!table) {
        return RISTRETTO_NOMEM;
    }
    
    // Add columns
    for (uint32_t i = 0; i < stmt->column_count; i++) {
        table_add_column(table, stmt->columns[i].name, stmt->columns[i].type);
    }
    
    // Register table
    if (!register_table(ctx->db, table)) {
        table_destroy(table);
        return RISTRETTO_ERROR;
    }
    
    return RISTRETTO_OK;
}

static RistrettoResult execute_insert(QueryContext* ctx) {
    Table* table = ctx->plan->table;
    Value* values = ctx->plan->data.insert.values;
    uint32_t value_count = ctx->plan->data.insert.value_count;
    
    // Check column count
    if (value_count != table->column_count) {
        return RISTRETTO_CONSTRAINT_ERROR;
    }
    
    // Check type compatibility
    for (uint32_t i = 0; i < value_count; i++) {
        DataType expected = table->columns[i].type;
        DataType actual = values[i].type;
        
        // Allow NULL for any type
        if (actual == TYPE_NULL) continue;
        
        // Check type match
        if (expected != actual) {
            // Allow some conversions
            if (expected == TYPE_REAL && actual == TYPE_INTEGER) {
                // Convert integer to real
                values[i].type = TYPE_REAL;
                values[i].value.real = (double)values[i].value.integer;
            } else {
                return RISTRETTO_CONSTRAINT_ERROR;
            }
        }
    }
    
    // Create row
    Row* row = row_create(table);
    if (!row) {
        return RISTRETTO_NOMEM;
    }
    
    // Set values
    for (uint32_t i = 0; i < value_count; i++) {
        row_set_value(row, table, i, &values[i]);
    }
    
    // Insert row into storage
    RowId row_id = table_insert_row(table, ctx->pager, row);
    row_destroy(row);
    
    if (row_id.page_id == 0) {
        return RISTRETTO_ERROR; // Failed to insert
    }
    
    return RISTRETTO_OK;
}

static char* value_to_string(Value* value) {
    if (!value) return strdup("NULL");
    
    switch (value->type) {
        case TYPE_NULL:
            return strdup("NULL");
        case TYPE_INTEGER: {
            char* str = malloc(32);
            snprintf(str, 32, "%lld", (long long)value->value.integer);
            return str;
        }
        case TYPE_REAL: {
            char* str = malloc(32);
            snprintf(str, 32, "%.6g", value->value.real);
            return str;
        }
        case TYPE_TEXT:
            return strdup(value->value.text.data ? value->value.text.data : "NULL");
        default:
            return strdup("?");
    }
}

static RistrettoResult execute_select(QueryContext* ctx) {
    Table* table = ctx->plan->table;
    
    if (!ctx->callback) {
        return RISTRETTO_OK; // No callback to send results to
    }
    
    // Prepare column names
    char** col_names = malloc(table->column_count * sizeof(char*));
    if (!col_names) return RISTRETTO_NOMEM;
    
    for (uint32_t i = 0; i < table->column_count; i++) {
        col_names[i] = table->columns[i].name;
    }
    
    // Scan the table
    TableScanner* scanner = table_scanner_create(table, ctx->pager);
    if (!scanner) {
        free(col_names);
        return RISTRETTO_NOMEM;
    }
    
    while (!table_scanner_at_end(scanner)) {
        Row* row = table_scanner_next(scanner);
        if (!row) break;
        
        // Convert row values to strings
        char** values = malloc(table->column_count * sizeof(char*));
        if (!values) {
            row_destroy(row);
            continue;
        }
        
        for (uint32_t i = 0; i < table->column_count; i++) {
            Value* val = row_get_value(row, table, i);
            values[i] = value_to_string(val);
            value_destroy(val);
        }
        
        // Call the callback
        ctx->callback(ctx->callback_ctx, table->column_count, values, col_names);
        
        // Clean up
        for (uint32_t i = 0; i < table->column_count; i++) {
            free(values[i]);
        }
        free(values);
        row_destroy(row);
    }
    
    table_scanner_destroy(scanner);
    free(col_names);
    return RISTRETTO_OK;
}

RistrettoResult execute_plan(QueryContext* ctx) {
    if (!ctx || !ctx->plan) {
        return RISTRETTO_ERROR;
    }
    
    switch (ctx->plan->type) {
        case PLAN_CREATE_TABLE:
            return execute_create_table(ctx);
            
        case PLAN_INSERT:
            return execute_insert(ctx);
            
        case PLAN_TABLE_SCAN:
            return execute_select(ctx);
            
        default:
            return RISTRETTO_ERROR;
    }
}

bool evaluate_expr(Expr* expr, Row* row, Table* table) {
    // TODO: Implement expression evaluation
    // For now, return false
    (void)expr;
    (void)row;
    (void)table;
    return false;
}
