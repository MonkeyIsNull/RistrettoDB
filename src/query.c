#include "query.h"
#include "simd.h"
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
    if (!db || !name) {
        return NULL;
    }
    
    TableCatalog* catalog = get_catalog(db);
    if (!catalog) {
        return NULL;
    }
    
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

// Forward declaration for SIMD-optimized SELECT execution
static RistrettoResult execute_select_simd(QueryContext* ctx);

// Check if WHERE clause can use primary index (equality on first INTEGER column)
static bool can_use_primary_index(Expr* filter, Table* table) {
    if (!filter || !table || table->column_count == 0) {
        return false;
    }
    
    // Check if first column is INTEGER type
    if (table->columns[0].type != TYPE_INTEGER) {
        return false;
    }
    
    // Check if filter is an equality condition on the first column
    if (filter->type == EXPR_BINARY_OP && filter->data.binary.op == OP_EQ) {
        Expr* left = filter->data.binary.left;
        Expr* right = filter->data.binary.right;
        
        // Check if one side is the first column and the other is a literal
        if (left->type == EXPR_COLUMN && right->type == EXPR_LITERAL) {
            if (strcmp(left->data.column.column, table->columns[0].name) == 0 && 
                right->data.literal.type == TYPE_INTEGER) {
                return true;
            }
        } else if (right->type == EXPR_COLUMN && left->type == EXPR_LITERAL) {
            if (strcmp(right->data.column.column, table->columns[0].name) == 0 && 
                left->data.literal.type == TYPE_INTEGER) {
                return true;
            }
        }
    }
    
    return false;
}

QueryPlan* plan_statement(Statement* stmt, RistrettoDB* db) {
    if (!stmt || !db) {
        return NULL;
    }
    
    QueryPlan* plan = calloc(1, sizeof(QueryPlan));
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
            plan->table = find_table(db, stmt->data.select.table_name);
            if (!plan->table) {
                free(plan);
                return NULL;
            }
            plan->data.scan.filter = stmt->data.select.where_clause;
            
            // Determine if we can use index scan
            bool can_use_index = false;
            if (plan->table->primary_index && plan->data.scan.filter) {
                // Check if WHERE clause has equality condition on first INTEGER column
                can_use_index = can_use_primary_index(plan->data.scan.filter, plan->table);
            }
            
            plan->type = can_use_index ? PLAN_INDEX_SCAN : PLAN_TABLE_SCAN;
            
            // Handle column selection properly
            if (stmt->data.select.column_count == UINT32_MAX) {
                // SELECT * - include all columns
                plan->data.scan.columns = NULL;
                plan->data.scan.column_count = 0; // 0 means all columns
            } else {
                // Specific column list
                plan->data.scan.column_count = stmt->data.select.column_count;
                if (plan->data.scan.column_count > 0) {
                    plan->data.scan.columns = malloc(plan->data.scan.column_count * sizeof(uint32_t));
                    if (!plan->data.scan.columns) {
                        free(plan);
                        return NULL;
                    }
                    
                    // Map column names to indices
                    for (uint32_t i = 0; i < plan->data.scan.column_count; i++) {
                        // Defensive: validate statement column array access
                        if (!stmt->data.select.columns || !stmt->data.select.columns[i]) {
                            free(plan->data.scan.columns);
                            free(plan);
                            return NULL;
                        }
                        
                        int col_idx = -1;
                        // Defensive: ensure table has columns and columns array exists
                        if (plan->table->columns && plan->table->column_count > 0) {
                            for (uint32_t j = 0; j < plan->table->column_count; j++) {
                                if (strcmp(plan->table->columns[j].name, stmt->data.select.columns[i]) == 0) {
                                    col_idx = j;
                                    break;
                                }
                            }
                        }
                        if (col_idx == -1) {
                            free(plan->data.scan.columns);
                            free(plan);
                            return NULL; // Column not found
                        }
                        plan->data.scan.columns[i] = col_idx;
                    }
                }
            }
            break;
            
        case STMT_SHOW_TABLES:
            plan->type = PLAN_SHOW_TABLES;
            plan->table = NULL;
            plan->data.show_tables.pattern = stmt->data.show_tables.pattern;
            break;
            
        case STMT_DESCRIBE:
            plan->type = PLAN_DESCRIBE;
            plan->table = find_table(db, stmt->data.describe.table_name);
            if (!plan->table) {
                free(plan);
                return NULL;
            }
            plan->data.describe.table_name = stmt->data.describe.table_name;
            break;
            
        case STMT_SHOW_CREATE_TABLE:
            plan->type = PLAN_SHOW_CREATE_TABLE;
            plan->table = find_table(db, stmt->data.show_create_table.table_name);
            if (!plan->table) {
                free(plan);
                return NULL;
            }
            plan->data.show_create_table.table_name = stmt->data.show_create_table.table_name;
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
            
            
        case PLAN_CREATE_TABLE:
            // CreateTableStmt is owned by the statement, not the plan
            break;
            
        case PLAN_SHOW_TABLES:
            // Pattern is owned by the statement, not the plan
            break;
            
        case PLAN_DESCRIBE:
            // Table name is owned by the statement, not the plan
            break;
            
        case PLAN_SHOW_CREATE_TABLE:
            // Table name is owned by the statement, not the plan
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
    Table* table = storage_table_create(stmt->table_name);
    if (!table) {
        return RISTRETTO_NOMEM;
    }
    
    // Add columns
    for (uint32_t i = 0; i < stmt->column_count; i++) {
        storage_table_add_column(table, stmt->columns[i].name, stmt->columns[i].type);
    }
    
    // Create primary index on first INTEGER column (if exists)
    for (uint32_t i = 0; i < table->column_count; i++) {
        if (table->columns[i].type == TYPE_INTEGER) {
            table->primary_index = btree_create(ctx->pager, table);
            break; // Only create index for first INTEGER column
        }
    }
    
    // Register table
    if (!register_table(ctx->db, table)) {
        storage_table_destroy(table);
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
    Row* row = storage_row_create(table);
    if (!row) {
        return RISTRETTO_NOMEM;
    }
    
    // Set values
    for (uint32_t i = 0; i < value_count; i++) {
        storage_row_set_value(row, table, i, &values[i]);
    }
    
    // Insert row into storage
    RowId row_id = table_insert_row(table, ctx->pager, row);
    
    if (row_id.page_id == 0) {
        storage_row_destroy(row);
        return RISTRETTO_ERROR; // Failed to insert
    }
    
    // Update index if it exists and there's an INTEGER column
    if (table->primary_index && table->column_count > 0 && table->columns[0].type == TYPE_INTEGER) {
        // Use the first INTEGER column value as the key
        uint32_t key = (uint32_t)values[0].value.integer; // Assume first column is INTEGER if index exists
        if (!btree_insert(table->primary_index, key, row_id)) {
            // Index insertion failed - this is a problem but we've already inserted the row
            // In a real system, we'd need transaction rollback here
        }
    }
    
    storage_row_destroy(row);
    return RISTRETTO_OK;
}

static char* value_to_string(Value* value) {
    if (!value) {
        char* result = malloc(5);
        if (result) strcpy(result, "NULL");
        return result;
    }
    
    switch (value->type) {
        case TYPE_NULL: {
            char* result = malloc(5);
            if (result) strcpy(result, "NULL");
            return result;
        }
        case TYPE_INTEGER: {
            char* str = malloc(32);
            if (!str) {
                char* result = malloc(5);
                if (result) strcpy(result, "NULL");
                return result;
            }
            snprintf(str, 32, "%lld", (long long)value->value.integer);
            return str;
        }
        case TYPE_REAL: {
            char* str = malloc(32);
            if (!str) {
                char* result = malloc(5);
                if (result) strcpy(result, "NULL");
                return result;
            }
            snprintf(str, 32, "%.6g", value->value.real);
            return str;
        }
        case TYPE_TEXT: {
            if (!value->value.text.data) {
                char* result = malloc(5);
                if (result) strcpy(result, "NULL");
                return result;
            }
            // Safety check for text length
            if (value->value.text.len > 10000) { // Sanity check
                char* result = malloc(16);
                if (result) strcpy(result, "[TEXT_TOO_LONG]");
                return result;
            }
            
            // Defensive: validate text data length against actual string length
            size_t actual_len = strlen(value->value.text.data);
            size_t safe_len = (actual_len < value->value.text.len) ? actual_len : value->value.text.len;
            if (safe_len > 10000) safe_len = 10000; // Additional safety cap
            
            char* result = malloc(safe_len + 1);
            if (!result) {
                // Fallback allocation for error case
                char* fallback = malloc(5);
                if (fallback) strcpy(fallback, "NULL");
                return fallback;
            }
            
            // Use strncpy for safer copying and ensure null termination
            strncpy(result, value->value.text.data, safe_len);
            result[safe_len] = '\0';
            return result;
        }
        default: {
            char* result = malloc(2);
            if (result) strcpy(result, "?");
            return result;
        }
    }
}

// Check if filter can be optimized with SIMD
static bool can_use_simd_filter(Expr* filter, Table* table) {
    if (!filter || !table || table->column_count == 0) {
        return false;
    }
    
    // Check for simple equality/comparison on INTEGER column
    if (filter->type == EXPR_BINARY_OP) {
        Expr* left = filter->data.binary.left;
        Expr* right = filter->data.binary.right;
        
        // Look for column comparison with literal
        if (left->type == EXPR_COLUMN && right->type == EXPR_LITERAL) {
            // Find column index and check if it's INTEGER
            for (uint32_t i = 0; i < table->column_count; i++) {
                if (strcmp(table->columns[i].name, left->data.column.column) == 0 &&
                    table->columns[i].type == TYPE_INTEGER &&
                    right->data.literal.type == TYPE_INTEGER &&
                    (filter->data.binary.op == OP_EQ || 
                     filter->data.binary.op == OP_GT || 
                     filter->data.binary.op == OP_LT)) {
                    return true;
                }
            }
        } else if (right->type == EXPR_COLUMN && left->type == EXPR_LITERAL) {
            // Find column index and check if it's INTEGER
            for (uint32_t i = 0; i < table->column_count; i++) {
                if (strcmp(table->columns[i].name, right->data.column.column) == 0 &&
                    table->columns[i].type == TYPE_INTEGER &&
                    left->data.literal.type == TYPE_INTEGER &&
                    (filter->data.binary.op == OP_EQ || 
                     filter->data.binary.op == OP_GT || 
                     filter->data.binary.op == OP_LT)) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

static RistrettoResult execute_select(QueryContext* ctx) {
    // Add comprehensive validation
    if (!ctx || !ctx->plan) {
        return RISTRETTO_ERROR;
    }
    
    Table* table = ctx->plan->table;
    if (!table || table->column_count == 0) {
        return RISTRETTO_ERROR;
    }
    
    if (!ctx->callback) {
        return RISTRETTO_OK; // No callback to send results to
    }
    
    // Check if we can use SIMD optimization
    bool use_simd = can_use_simd_filter(ctx->plan->data.scan.filter, table);
    if (use_simd && table->row_count > 100) {
        return execute_select_simd(ctx);
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
            storage_row_destroy(row);
            continue;
        }
        
        // Initialize all values to NULL for safety
        for (uint32_t i = 0; i < table->column_count; i++) {
            values[i] = NULL;
        }
        
        bool row_valid = true;
        for (uint32_t i = 0; i < table->column_count; i++) {
            Value* val = storage_row_get_value(row, table, i);
            if (val) {
                values[i] = value_to_string(val);
                storage_value_destroy(val);
            } else {
                values[i] = malloc(5);
                if (values[i]) {
                    strcpy(values[i], "NULL");
                }
            }
            
            // Check if allocation failed
            if (!values[i]) {
                row_valid = false;
                break;
            }
        }
        
        // Call the callback only if row is valid
        if (row_valid) {
            ctx->callback(ctx->callback_ctx, table->column_count, values, col_names);
        }
        
        // Clean up - safely handle partially allocated arrays
        for (uint32_t i = 0; i < table->column_count; i++) {
            if (values[i]) {
                free(values[i]);
            }
        }
        free(values);
        storage_row_destroy(row);
    }
    
    table_scanner_destroy(scanner);
    free(col_names);
    return RISTRETTO_OK;
}

static RistrettoResult execute_select_simd(QueryContext* ctx) {
    Table* table = ctx->plan->table;
    Expr* filter = ctx->plan->data.scan.filter;
    
    if (!filter || filter->type != EXPR_BINARY_OP) {
        return RISTRETTO_ERROR; // Should not happen given can_use_simd_filter check
    }
    
    // Extract filter information
    Expr* left = filter->data.binary.left;
    Expr* right = filter->data.binary.right;
    uint32_t col_index = UINT32_MAX;
    int64_t compare_value = 0;
    BinaryOp op = filter->data.binary.op;
    
    // Find the column and value
    if (left->type == EXPR_COLUMN && right->type == EXPR_LITERAL) {
        for (uint32_t i = 0; i < table->column_count; i++) {
            if (strcmp(table->columns[i].name, left->data.column.column) == 0) {
                col_index = i;
                compare_value = right->data.literal.value.integer;
                break;
            }
        }
    } else if (right->type == EXPR_COLUMN && left->type == EXPR_LITERAL) {
        for (uint32_t i = 0; i < table->column_count; i++) {
            if (strcmp(table->columns[i].name, right->data.column.column) == 0) {
                col_index = i;
                compare_value = left->data.literal.value.integer;
                // Flip operator for reversed operands
                if (op == OP_GT) op = OP_LT;
                else if (op == OP_LT) op = OP_GT;
                break;
            }
        }
    }
    
    if (col_index == UINT32_MAX) {
        return RISTRETTO_ERROR; // Column not found
    }
    
    // Prepare column names
    char** col_names = malloc(table->column_count * sizeof(char*));
    if (!col_names) return RISTRETTO_NOMEM;
    
    for (uint32_t i = 0; i < table->column_count; i++) {
        col_names[i] = table->columns[i].name;
    }
    
    // Create arrays to store column data and bitmap
    uint8_t* bitmap = malloc(table->row_count);
    if (!bitmap) {
        free(col_names);
        return RISTRETTO_NOMEM;
    }
    
    // Extract column data into contiguous array for SIMD processing
    int64_t* column_data = malloc(table->row_count * sizeof(int64_t));
    if (!column_data) {
        free(bitmap);
        free(col_names);
        return RISTRETTO_NOMEM;
    }
    
    // Scan table and extract column values
    TableScanner* scanner = table_scanner_create(table, ctx->pager);
    if (!scanner) {
        free(column_data);
        free(bitmap);
        free(col_names);
        return RISTRETTO_NOMEM;
    }
    
    uint32_t row_index = 0;
    while (!table_scanner_at_end(scanner) && row_index < table->row_count) {
        Row* row = table_scanner_next(scanner);
        if (!row) break;
        
        Value* val = storage_row_get_value(row, table, col_index);
        if (val && val->type == TYPE_INTEGER) {
            column_data[row_index] = val->value.integer;
            storage_value_destroy(val);
        } else {
            column_data[row_index] = 0; // Default for NULL/invalid values
            if (val) storage_value_destroy(val);
        }
        
        storage_row_destroy(row);
        row_index++;
    }
    
    table_scanner_destroy(scanner);
    uint32_t actual_rows = row_index;
    
    // Apply SIMD filtering
    switch (op) {
        case OP_EQ:
            simd_filter_eq_i64(column_data, actual_rows, compare_value, bitmap);
            break;
        case OP_GT:
            simd_filter_gt_i64(column_data, actual_rows, compare_value, bitmap);
            break;
        case OP_LT:
            simd_filter_lt_i64(column_data, actual_rows, compare_value, bitmap);
            break;
        default:
            // Fallback to regular scan
            free(column_data);
            free(bitmap);
            free(col_names);
            return execute_select(ctx);
    }
    
    // Scan table again and return only matching rows
    scanner = table_scanner_create(table, ctx->pager);
    if (!scanner) {
        free(column_data);
        free(bitmap);
        free(col_names);
        return RISTRETTO_NOMEM;
    }
    
    row_index = 0;
    while (!table_scanner_at_end(scanner) && row_index < actual_rows) {
        Row* row = table_scanner_next(scanner);
        if (!row) break;
        
        // Check if this row matches the filter
        if (bitmap[row_index]) {
            // Convert row values to strings
            char** values = malloc(table->column_count * sizeof(char*));
            if (values) {
                bool row_valid = true;
                for (uint32_t i = 0; i < table->column_count; i++) {
                    Value* val = storage_row_get_value(row, table, i);
                    if (val) {
                        values[i] = value_to_string(val);
                        storage_value_destroy(val);
                    } else {
                        values[i] = malloc(5);
                        if (values[i]) strcpy(values[i], "NULL");
                    }
                    
                    if (!values[i]) {
                        row_valid = false;
                        break;
                    }
                }
                
                if (row_valid) {
                    ctx->callback(ctx->callback_ctx, table->column_count, values, col_names);
                }
                
                // Clean up
                for (uint32_t i = 0; i < table->column_count; i++) {
                    if (values[i]) free(values[i]);
                }
                free(values);
            }
        }
        
        storage_row_destroy(row);
        row_index++;
    }
    
    table_scanner_destroy(scanner);
    free(column_data);
    free(bitmap);
    free(col_names);
    return RISTRETTO_OK;
}

static RistrettoResult execute_index_scan(QueryContext* ctx) {
    // Add comprehensive validation
    if (!ctx || !ctx->plan || !ctx->plan->table) {
        return RISTRETTO_ERROR;
    }
    
    Table* table = ctx->plan->table;
    if (!table->primary_index || table->column_count == 0) {
        return RISTRETTO_ERROR; // No index or empty table
    }
    
    if (!ctx->callback) {
        return RISTRETTO_OK; // No callback to send results to
    }
    
    // Extract the search key from the WHERE clause
    Expr* filter = ctx->plan->data.scan.filter;
    uint32_t search_key = 0;
    bool key_found = false;
    
    if (filter && filter->type == EXPR_BINARY_OP && filter->data.binary.op == OP_EQ) {
        Expr* left = filter->data.binary.left;
        Expr* right = filter->data.binary.right;
        
        // Extract integer literal value
        if (left->type == EXPR_COLUMN && right->type == EXPR_LITERAL && 
            right->data.literal.type == TYPE_INTEGER) {
            search_key = (uint32_t)right->data.literal.value.integer;
            key_found = true;
        } else if (right->type == EXPR_COLUMN && left->type == EXPR_LITERAL && 
                   left->data.literal.type == TYPE_INTEGER) {
            search_key = (uint32_t)left->data.literal.value.integer;
            key_found = true;
        }
    }
    
    if (!key_found) {
        return RISTRETTO_ERROR; // Invalid WHERE clause for index scan
    }
    
    // Use B-tree to find the row
    RowId* row_id_ptr = btree_find(table->primary_index, search_key);
    if (!row_id_ptr) {
        return RISTRETTO_OK; // No matching row found - this is not an error
    }
    
    RowId row_id = *row_id_ptr;
    
    // Prepare column names
    char** col_names = malloc(table->column_count * sizeof(char*));
    if (!col_names) return RISTRETTO_NOMEM;
    
    for (uint32_t i = 0; i < table->column_count; i++) {
        col_names[i] = table->columns[i].name;
    }
    
    // Get the specific row
    Row* row = table_get_row(table, ctx->pager, row_id);
    if (row) {
        // Convert row values to strings
        char** values = malloc(table->column_count * sizeof(char*));
        if (values) {
            // Initialize all values to NULL for safety
            for (uint32_t i = 0; i < table->column_count; i++) {
                values[i] = NULL;
            }
            
            bool row_valid = true;
            for (uint32_t i = 0; i < table->column_count; i++) {
                Value* val = storage_row_get_value(row, table, i);
                if (val) {
                    values[i] = value_to_string(val);
                    storage_value_destroy(val);
                } else {
                    values[i] = malloc(5);
                    if (values[i]) {
                        strcpy(values[i], "NULL");
                    }
                }
                
                // Check if allocation failed
                if (!values[i]) {
                    row_valid = false;
                    break;
                }
            }
            
            // Call the callback only if row is valid
            if (row_valid) {
                ctx->callback(ctx->callback_ctx, table->column_count, values, col_names);
            }
            
            // Clean up - safely handle partially allocated arrays
            for (uint32_t i = 0; i < table->column_count; i++) {
                if (values[i]) {
                    free(values[i]);
                }
            }
            free(values);
        }
        storage_row_destroy(row);
    }
    
    free(col_names);
    return RISTRETTO_OK;
}

static RistrettoResult execute_show_tables(QueryContext* ctx) {
    TableCatalog* catalog = get_catalog(ctx->db);
    
    if (!ctx->callback) {
        return RISTRETTO_OK;
    }
    
    // Prepare column names for SHOW TABLES output
    char* col_names[] = {"Tables_in_database"};
    
    for (uint32_t i = 0; i < catalog->count; i++) {
        const char* table_name = catalog->entries[i].name;
        
        // Apply pattern filter if specified
        if (ctx->plan->data.show_tables.pattern) {
            // Simple pattern matching - for now just exact match or basic wildcard
            const char* pattern = ctx->plan->data.show_tables.pattern;
            bool matches = false;
            
            if (strcmp(pattern, "%") == 0) {
                matches = true; // Match all
            } else if (strstr(pattern, "%") != NULL) {
                // Basic wildcard support - prefix match
                char* percent_pos = strstr(pattern, "%");
                size_t prefix_len = percent_pos - pattern;
                matches = (strncmp(table_name, pattern, prefix_len) == 0);
            } else {
                matches = (strcmp(table_name, pattern) == 0);
            }
            
            if (!matches) continue;
        }
        
        // Create result row
        char* values[] = {(char*)table_name};
        ctx->callback(ctx->callback_ctx, 1, values, col_names);
    }
    
    return RISTRETTO_OK;
}

static RistrettoResult execute_describe(QueryContext* ctx) {
    Table* table = ctx->plan->table;
    if (!table) {
        return RISTRETTO_ERROR;
    }
    
    if (!ctx->callback) {
        return RISTRETTO_OK;
    }
    
    // Prepare column names for DESCRIBE output (MySQL/PostgreSQL style)
    char* col_names[] = {"Field", "Type", "Null", "Key", "Default", "Extra"};
    
    for (uint32_t i = 0; i < table->column_count; i++) {
        const char* field_name = table->columns[i].name;
        const char* type_name;
        
        // Convert internal type to string
        switch (table->columns[i].type) {
            case TYPE_INTEGER:
                type_name = "INTEGER";
                break;
            case TYPE_REAL:
                type_name = "REAL";
                break;
            case TYPE_TEXT:
                type_name = "TEXT";
                break;
            default:
                type_name = "UNKNOWN";
                break;
        }
        
        // Create result row (simplified - no null/key/default/extra info for now)
        char* values[] = {
            (char*)field_name,
            (char*)type_name,
            (char*)"YES",  // Null
            (char*)"",     // Key
            (char*)"",     // Default
            (char*)""      // Extra
        };
        
        ctx->callback(ctx->callback_ctx, 6, values, col_names);
    }
    
    return RISTRETTO_OK;
}

static RistrettoResult execute_show_create_table(QueryContext* ctx) {
    Table* table = ctx->plan->table;
    if (!table) {
        return RISTRETTO_ERROR;
    }
    
    if (!ctx->callback) {
        return RISTRETTO_OK;
    }
    
    // Prepare column names for SHOW CREATE TABLE output
    char* col_names[] = {"Table", "Create Table"};
    
    // Generate CREATE TABLE statement
    char* create_stmt = malloc(4096); // Large buffer for CREATE TABLE statement
    if (!create_stmt) {
        return RISTRETTO_NOMEM;
    }
    
    // Start building the CREATE TABLE statement
    int pos = snprintf(create_stmt, 4096, "CREATE TABLE %s (\n", table->name);
    
    // Add columns
    for (uint32_t i = 0; i < table->column_count; i++) {
        const char* type_name;
        switch (table->columns[i].type) {
            case TYPE_INTEGER:
                type_name = "INTEGER";
                break;
            case TYPE_REAL:
                type_name = "REAL";
                break;
            case TYPE_TEXT:
                type_name = "TEXT";
                break;
            default:
                type_name = "UNKNOWN";
                break;
        }
        
        if (i > 0) {
            pos += snprintf(create_stmt + pos, 4096 - pos, ",\n");
        }
        pos += snprintf(create_stmt + pos, 4096 - pos, "  %s %s", 
                       table->columns[i].name, type_name);
    }
    
    // Close the statement
    snprintf(create_stmt + pos, 4096 - pos, "\n)");
    
    // Create result row
    char* values[] = {table->name, create_stmt};
    ctx->callback(ctx->callback_ctx, 2, values, col_names);
    
    free(create_stmt);
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
            
        case PLAN_INDEX_SCAN:
            return execute_index_scan(ctx);
            
        case PLAN_SHOW_TABLES:
            return execute_show_tables(ctx);
            
        case PLAN_DESCRIBE:
            return execute_describe(ctx);
            
        case PLAN_SHOW_CREATE_TABLE:
            return execute_show_create_table(ctx);
            
        default:
            return RISTRETTO_ERROR;
    }
}

// Helper function for expression to value conversion
static Value* evaluate_expr_to_value(Expr* expr, Row* row, Table* table);

// Helper function for value comparison
static int storage_value_compare(Value* left, Value* right) {
    if (left->type != right->type) return -1; // Type mismatch
    
    switch (left->type) {
        case TYPE_INTEGER:
            if (left->value.integer < right->value.integer) return -1;
            if (left->value.integer > right->value.integer) return 1;
            return 0;
        case TYPE_REAL:
            if (left->value.real < right->value.real) return -1;
            if (left->value.real > right->value.real) return 1;
            return 0;
        case TYPE_TEXT:
            return strcmp(left->value.text.data, right->value.text.data);
        case TYPE_NULL:
            return 0;
        default:
            return -1;
    }
}

static bool evaluate_comparison(Expr* expr, Row* row, Table* table) {
    Value* left_val = evaluate_expr_to_value(expr->data.binary.left, row, table);
    Value* right_val = evaluate_expr_to_value(expr->data.binary.right, row, table);
    
    if (!left_val || !right_val) {
        storage_value_destroy(left_val);
        storage_value_destroy(right_val);
        return false;
    }
    
    int cmp = storage_value_compare(left_val, right_val);
    bool result = false;
    
    switch (expr->data.binary.op) {
        case OP_EQ: result = (cmp == 0); break;
        case OP_NE: result = (cmp != 0); break;
        case OP_LT: result = (cmp < 0); break;
        case OP_LE: result = (cmp <= 0); break;
        case OP_GT: result = (cmp > 0); break;
        case OP_GE: result = (cmp >= 0); break;
        default: result = false; break;
    }
    
    storage_value_destroy(left_val);
    storage_value_destroy(right_val);
    return result;
}

static Value* evaluate_expr_to_value(Expr* expr, Row* row, Table* table) {
    if (!expr) return NULL;
    
    switch (expr->type) {
        case EXPR_LITERAL: {
            Value* val = malloc(sizeof(Value));
            if (val) {
                *val = expr->data.literal;
                // For text values, make a copy of the string
                if (val->type == TYPE_TEXT && val->value.text.data) {
                    val->value.text.data = strdup(val->value.text.data);
                }
            }
            return val;
        }
        
        case EXPR_COLUMN: {
            // Find column index
            int col_idx = -1;
            for (uint32_t i = 0; i < table->column_count; i++) {
                if (strcmp(table->columns[i].name, expr->data.column.column) == 0) {
                    col_idx = i;
                    break;
                }
            }
            
            if (col_idx == -1) return NULL; // Column not found
            
            return storage_row_get_value(row, table, col_idx);
        }
        
        default:
            return NULL;
    }
}

bool evaluate_expr(Expr* expr, Row* row, Table* table) {
    if (!expr) return true; // No filter means include all rows
    
    switch (expr->type) {
        case EXPR_LITERAL:
            // Literals are always true in boolean context (SQL semantics)
            return expr->data.literal.type != TYPE_NULL;
            
        case EXPR_COLUMN: {
            // Find column index
            int col_idx = -1;
            for (uint32_t i = 0; i < table->column_count; i++) {
                if (strcmp(table->columns[i].name, expr->data.column.column) == 0) {
                    col_idx = i;
                    break;
                }
            }
            
            if (col_idx == -1) return false; // Column not found
            
            Value* val = storage_row_get_value(row, table, col_idx);
            bool result = (val && val->type != TYPE_NULL);
            storage_value_destroy(val);
            return result;
        }
        
        case EXPR_BINARY_OP: {
            switch (expr->data.binary.op) {
                case OP_AND: {
                    bool left_result = evaluate_expr(expr->data.binary.left, row, table);
                    bool right_result = evaluate_expr(expr->data.binary.right, row, table);
                    return left_result && right_result;
                }
                case OP_OR: {
                    bool left_result = evaluate_expr(expr->data.binary.left, row, table);
                    bool right_result = evaluate_expr(expr->data.binary.right, row, table);
                    return left_result || right_result;
                }
                case OP_EQ:
                case OP_NE:
                case OP_LT:
                case OP_LE:
                case OP_GT:
                case OP_GE:
                    return evaluate_comparison(expr, row, table);
                default:
                    return false;
            }
        }
        
        default:
            return false;
    }
}
