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
    PLAN_UPDATE,
    PLAN_DELETE,
    PLAN_CREATE_TABLE
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
            Expr *filter;
            uint32_t *columns;
            Value *values;
            uint32_t update_count;
        } update;
        struct {
            Expr *filter;
        } delete;
        struct {
            CreateTableStmt *stmt;
        } create_table;
    } data;
} QueryPlan;

typedef struct {
    RistrettoDB *db;
    QueryPlan *plan;
    RistrettoCallback callback;
    void *callback_ctx;
} QueryContext;

QueryPlan* plan_statement(Statement *stmt, RistrettoDB *db);
void plan_destroy(QueryPlan *plan);

RistrettoResult execute_plan(QueryContext *ctx);

bool evaluate_expr(Expr *expr, Row *row, Table *table);

#endif
