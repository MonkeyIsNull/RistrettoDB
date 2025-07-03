#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

typedef struct {
    const char* start;
    const char* current;
    size_t length;
} Scanner;

static void scanner_init(Scanner* scanner, const char* sql) {
    scanner->start = sql;
    scanner->current = sql;
    scanner->length = strlen(sql);
}

static bool is_at_end(Scanner* scanner) {
    return scanner->current >= scanner->start + scanner->length;
}

static char peek(Scanner* scanner) {
    if (is_at_end(scanner)) return '\0';
    return *scanner->current;
}

static char advance(Scanner* scanner) {
    if (is_at_end(scanner)) return '\0';
    return *scanner->current++;
}

static void skip_whitespace(Scanner* scanner) {
    while (!is_at_end(scanner)) {
        char c = peek(scanner);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(scanner);
        } else {
            break;
        }
    }
}

static bool match_keyword(Scanner* scanner, const char* keyword) {
    skip_whitespace(scanner);
    const char* saved = scanner->current;
    size_t len = strlen(keyword);
    
    for (size_t i = 0; i < len; i++) {
        if (toupper(advance(scanner)) != toupper(keyword[i])) {
            scanner->current = saved;
            return false;
        }
    }
    
    // Ensure keyword ends with non-alpha character
    if (isalpha(peek(scanner))) {
        scanner->current = saved;
        return false;
    }
    
    return true;
}

static char* parse_identifier(Scanner* scanner) {
    skip_whitespace(scanner);
    const char* start = scanner->current;
    
    if (!isalpha(peek(scanner)) && peek(scanner) != '_') {
        return NULL;
    }
    
    while (isalnum(peek(scanner)) || peek(scanner) == '_') {
        advance(scanner);
    }
    
    size_t len = scanner->current - start;
    char* identifier = malloc(len + 1);
    if (identifier) {
        memcpy(identifier, start, len);
        identifier[len] = '\0';
    }
    
    return identifier;
}

static Value* parse_value(Scanner* scanner) {
    skip_whitespace(scanner);
    Value* value = malloc(sizeof(Value));
    if (!value) return NULL;
    
    char c = peek(scanner);
    
    // Parse string literal
    if (c == '\'' || c == '"') {
        char quote = advance(scanner);
        const char* start = scanner->current;
        
        while (peek(scanner) != quote && !is_at_end(scanner)) {
            advance(scanner);
        }
        
        size_t len = scanner->current - start;
        value->type = TYPE_TEXT;
        value->value.text.len = len;
        value->value.text.data = malloc(len + 1);
        if (value->value.text.data) {
            memcpy(value->value.text.data, start, len);
            value->value.text.data[len] = '\0';
        }
        
        advance(scanner); // Skip closing quote
        return value;
    }
    
    // Parse number
    if (isdigit(c) || c == '-' || c == '+') {
        const char* start = scanner->current;
        bool has_decimal = false;
        
        if (c == '-' || c == '+') advance(scanner);
        
        while (isdigit(peek(scanner))) {
            advance(scanner);
        }
        
        if (peek(scanner) == '.') {
            has_decimal = true;
            advance(scanner);
            while (isdigit(peek(scanner))) {
                advance(scanner);
            }
        }
        
        char* end;
        if (has_decimal) {
            value->type = TYPE_REAL;
            value->value.real = strtod(start, &end);
        } else {
            value->type = TYPE_INTEGER;
            value->value.integer = strtoll(start, &end, 10);
        }
        
        return value;
    }
    
    // Parse NULL
    if (match_keyword(scanner, "NULL")) {
        value->type = TYPE_NULL;
        return value;
    }
    
    free(value);
    return NULL;
}

static DataType parse_type(Scanner* scanner) {
    skip_whitespace(scanner);
    
    if (match_keyword(scanner, "INTEGER") || match_keyword(scanner, "INT")) {
        return TYPE_INTEGER;
    } else if (match_keyword(scanner, "REAL") || match_keyword(scanner, "FLOAT") || 
               match_keyword(scanner, "DOUBLE")) {
        return TYPE_REAL;
    } else if (match_keyword(scanner, "TEXT") || match_keyword(scanner, "VARCHAR")) {
        return TYPE_TEXT;
    }
    
    return TYPE_NULL;
}

static bool expect_char(Scanner* scanner, char expected) {
    skip_whitespace(scanner);
    if (peek(scanner) == expected) {
        advance(scanner);
        return true;
    }
    return false;
}

static Statement* parse_create_table(Scanner* scanner) {
    Statement* stmt = malloc(sizeof(Statement));
    if (!stmt) return NULL;
    
    stmt->type = STMT_CREATE_TABLE;
    stmt->data.create_table.table_name = parse_identifier(scanner);
    if (!stmt->data.create_table.table_name) {
        free(stmt);
        return NULL;
    }
    
    if (!expect_char(scanner, '(')) {
        free(stmt->data.create_table.table_name);
        free(stmt);
        return NULL;
    }
    
    // Parse columns
    stmt->data.create_table.columns = NULL;
    stmt->data.create_table.column_count = 0;
    size_t capacity = 0;
    
    do {
        if (stmt->data.create_table.column_count >= capacity) {
            capacity = capacity ? capacity * 2 : 4;
            void* new_cols = realloc(stmt->data.create_table.columns,
                                     capacity * sizeof(stmt->data.create_table.columns[0]));
            if (!new_cols) {
                statement_destroy(stmt);
                return NULL;
            }
            stmt->data.create_table.columns = new_cols;
        }
        
        size_t idx = stmt->data.create_table.column_count;
        stmt->data.create_table.columns[idx].name = parse_identifier(scanner);
        if (!stmt->data.create_table.columns[idx].name) {
            statement_destroy(stmt);
            return NULL;
        }
        
        stmt->data.create_table.columns[idx].type = parse_type(scanner);
        if (stmt->data.create_table.columns[idx].type == TYPE_NULL) {
            statement_destroy(stmt);
            return NULL;
        }
        
        stmt->data.create_table.column_count++;
        
    } while (expect_char(scanner, ','));
    
    if (!expect_char(scanner, ')')) {
        statement_destroy(stmt);
        return NULL;
    }
    
    return stmt;
}

static Statement* parse_insert(Scanner* scanner) {
    Statement* stmt = malloc(sizeof(Statement));
    if (!stmt) return NULL;
    
    stmt->type = STMT_INSERT;
    
    if (!match_keyword(scanner, "INTO")) {
        free(stmt);
        return NULL;
    }
    
    stmt->data.insert.table_name = parse_identifier(scanner);
    if (!stmt->data.insert.table_name) {
        free(stmt);
        return NULL;
    }
    
    if (!match_keyword(scanner, "VALUES")) {
        free(stmt->data.insert.table_name);
        free(stmt);
        return NULL;
    }
    
    if (!expect_char(scanner, '(')) {
        free(stmt->data.insert.table_name);
        free(stmt);
        return NULL;
    }
    
    // Parse values
    stmt->data.insert.values = NULL;
    stmt->data.insert.value_count = 0;
    size_t capacity = 0;
    
    do {
        if (stmt->data.insert.value_count >= capacity) {
            capacity = capacity ? capacity * 2 : 4;
            Value* new_vals = realloc(stmt->data.insert.values,
                                      capacity * sizeof(Value));
            if (!new_vals) {
                statement_destroy(stmt);
                return NULL;
            }
            stmt->data.insert.values = new_vals;
        }
        
        Value* val = parse_value(scanner);
        if (!val) {
            statement_destroy(stmt);
            return NULL;
        }
        
        stmt->data.insert.values[stmt->data.insert.value_count++] = *val;
        free(val);
        
    } while (expect_char(scanner, ','));
    
    if (!expect_char(scanner, ')')) {
        statement_destroy(stmt);
        return NULL;
    }
    
    return stmt;
}

static Statement* parse_select(Scanner* scanner) {
    Statement* stmt = malloc(sizeof(Statement));
    if (!stmt) return NULL;
    
    stmt->type = STMT_SELECT;
    stmt->data.select.columns = NULL;
    stmt->data.select.column_count = 0;
    stmt->data.select.where_clause = NULL;
    
    // Parse column list or *
    skip_whitespace(scanner);
    if (peek(scanner) == '*') {
        advance(scanner);
        stmt->data.select.column_count = 0;
    } else {
        // TODO: Parse column list
    }
    
    if (!match_keyword(scanner, "FROM")) {
        statement_destroy(stmt);
        return NULL;
    }
    
    stmt->data.select.table_name = parse_identifier(scanner);
    if (!stmt->data.select.table_name) {
        statement_destroy(stmt);
        return NULL;
    }
    
    // TODO: Parse WHERE clause
    
    return stmt;
}

Statement* parse_sql(const char* sql) {
    Scanner scanner;
    scanner_init(&scanner, sql);
    
    skip_whitespace(&scanner);
    
    if (match_keyword(&scanner, "CREATE")) {
        if (match_keyword(&scanner, "TABLE")) {
            return parse_create_table(&scanner);
        }
    } else if (match_keyword(&scanner, "INSERT")) {
        return parse_insert(&scanner);
    } else if (match_keyword(&scanner, "SELECT")) {
        return parse_select(&scanner);
    }
    
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