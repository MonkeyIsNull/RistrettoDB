#ifndef RISTRETTO_PARSER_H
#define RISTRETTO_PARSER_H

#include <stdbool.h>
#include "storage.h"

typedef enum {
    STMT_CREATE_TABLE,
    STMT_INSERT,
    STMT_SELECT,
    STMT_SHOW_TABLES,
    STMT_DESCRIBE,
    STMT_SHOW_CREATE_TABLE
} StatementType;

typedef enum {
    EXPR_LITERAL,
    EXPR_COLUMN,
    EXPR_BINARY_OP
} ExprType;

typedef enum {
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,
    OP_AND,
    OP_OR
} BinaryOp;

typedef struct Expr {
    ExprType type;
    union {
        Value literal;
        struct {
            char *table;
            char *column;
        } column;
        struct {
            BinaryOp op;
            struct Expr *left;
            struct Expr *right;
        } binary;
    } data;
} Expr;

typedef struct {
    char *table_name;
    uint32_t column_count;
    struct {
        char *name;
        DataType type;
    } *columns;
} CreateTableStmt;

typedef struct {
    char *table_name;
    uint32_t value_count;
    Value *values;
} InsertStmt;

typedef struct {
    char *table_name;
    uint32_t column_count;
    char **columns;
    Expr *where_clause;
} SelectStmt;

typedef struct {
    char *pattern; // Optional LIKE pattern
} ShowTablesStmt;

typedef struct {
    char *table_name;
} DescribeStmt;

typedef struct {
    char *table_name;
} ShowCreateTableStmt;

typedef struct {
    StatementType type;
    union {
        CreateTableStmt create_table;
        InsertStmt insert;
        SelectStmt select;
        ShowTablesStmt show_tables;
        DescribeStmt describe;
        ShowCreateTableStmt show_create_table;
    } data;
} Statement;

Statement* parse_sql(const char *sql);
void statement_destroy(Statement *stmt);

void expr_destroy(Expr *expr);

#endif
