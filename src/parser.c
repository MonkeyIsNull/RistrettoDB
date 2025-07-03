#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

Statement* parse_sql(const char* sql) {
    // TODO: Implement proper SQL parsing
    // For now, return NULL to indicate parse error
    (void)sql;
    return NULL;
}

void statement_destroy(Statement* stmt) {
    if (!stmt) {
        return;
    }
    
    switch (stmt->type) {
        case STMT_CREATE_TABLE:
            free(stmt->data.create_table.table_name);
            for (uint32_t i = 0; i < stmt->data.create_table.column_count; i++) {
                free(stmt->data.create_table.columns[i].name);
            }
            free(stmt->data.create_table.columns);
            break;
            
        case STMT_INSERT:
            free(stmt->data.insert.table_name);
            for (uint32_t i = 0; i < stmt->data.insert.value_count; i++) {
                if (stmt->data.insert.values[i].type == TYPE_TEXT) {
                    free(stmt->data.insert.values[i].value.text.data);
                }
            }
            free(stmt->data.insert.values);
            break;
            
        case STMT_SELECT:
            free(stmt->data.select.table_name);
            for (uint32_t i = 0; i < stmt->data.select.column_count; i++) {
                free(stmt->data.select.columns[i]);
            }
            free(stmt->data.select.columns);
            expr_destroy(stmt->data.select.where_clause);
            break;
            
        case STMT_UPDATE:
            free(stmt->data.update.table_name);
            for (uint32_t i = 0; i < stmt->data.update.update_count; i++) {
                free(stmt->data.update.updates[i].column);
                if (stmt->data.update.updates[i].value.type == TYPE_TEXT) {
                    free(stmt->data.update.updates[i].value.value.text.data);
                }
            }
            free(stmt->data.update.updates);
            expr_destroy(stmt->data.update.where_clause);
            break;
            
        case STMT_DELETE:
            free(stmt->data.delete.table_name);
            expr_destroy(stmt->data.delete.where_clause);
            break;
    }
    
    free(stmt);
}

void expr_destroy(Expr* expr) {
    if (!expr) {
        return;
    }
    
    switch (expr->type) {
        case EXPR_LITERAL:
            if (expr->data.literal.type == TYPE_TEXT) {
                free(expr->data.literal.value.text.data);
            }
            break;
            
        case EXPR_COLUMN:
            free(expr->data.column.table);
            free(expr->data.column.column);
            break;
            
        case EXPR_BINARY_OP:
            expr_destroy(expr->data.binary.left);
            expr_destroy(expr->data.binary.right);
            break;
    }
    
    free(expr);
}
