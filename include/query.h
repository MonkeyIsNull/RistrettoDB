#ifndef RISTRETTO_QUERY_H
#define RISTRETTO_QUERY_H

#include "parser.h"
#include "storage.h"
#include "btree.h"
#include "db.h"

typedef enum {
    PLAN_TABLE_SCAN,
    PLAN_INDEX_SCAN,
    PLAN_INSERT,
    PLAN_CREATE_TABLE,
    PLAN_SHOW_TABLES,
    PLAN_DESCRIBE,
    PLAN_SHOW_CREATE_TABLE
} PlanType;

typedef struct QueryPlan {
    PlanType type;
    Table *table;
    union {
        struct {
            Expr *filter;
            uint32_t *columns;
            uint32_t column_count;
        } scan;
        struct {
            Value *values;
            uint32_t value_count;
        } insert;
        struct {
            CreateTableStmt *stmt;
        } create_table;
        struct {
            char *pattern;
        } show_tables;
        struct {
            char *table_name;
        } describe;
        struct {
            char *table_name;
        } show_create_table;
    } data;
} QueryPlan;

typedef struct {
    RistrettoDB *db;
    Pager *pager;
    QueryPlan *plan;
    RistrettoCallback callback;
    void *callback_ctx;
} QueryContext;

QueryPlan* plan_statement(Statement *stmt, RistrettoDB *db);
void plan_destroy(QueryPlan *plan);

RistrettoResult execute_plan(QueryContext *ctx);

bool evaluate_expr(Expr *expr, Row *row, Table *table);

#endif
