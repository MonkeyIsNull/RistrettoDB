#include "db.h"
#include "pager.h"
#include "parser.h"
#include "query.h"
#include <stdlib.h>
#include <string.h>

struct RistrettoDB {
    Pager* pager;
    Table** tables;
    uint32_t table_count;
    uint32_t table_capacity;
};

RistrettoDB* ristretto_open(const char* filename) {
    RistrettoDB* db = malloc(sizeof(RistrettoDB));
    if (!db) {
        return NULL;
    }
    
    db->pager = pager_open(filename);
    if (!db->pager) {
        free(db);
        return NULL;
    }
    
    db->tables = NULL;
    db->table_count = 0;
    db->table_capacity = 0;
    
    return db;
}

void ristretto_close(RistrettoDB* db) {
    if (!db) {
        return;
    }
    
    if (db->pager) {
        pager_close(db->pager);
    }
    
    for (uint32_t i = 0; i < db->table_count; i++) {
        table_destroy(db->tables[i]);
    }
    free(db->tables);
    
    free(db);
}

RistrettoResult ristretto_exec(RistrettoDB* db, const char* sql) {
    Statement* stmt = parse_sql(sql);
    if (!stmt) {
        return RISTRETTO_PARSE_ERROR;
    }
    
    QueryPlan* plan = plan_statement(stmt, db);
    if (!plan) {
        statement_destroy(stmt);
        return RISTRETTO_ERROR;
    }
    
    QueryContext ctx = {
        .db = db,
        .pager = db->pager,
        .plan = plan,
        .callback = NULL,
        .callback_ctx = NULL
    };
    
    RistrettoResult result = execute_plan(&ctx);
    
    plan_destroy(plan);
    statement_destroy(stmt);
    
    return result;
}

RistrettoResult ristretto_query(RistrettoDB* db, const char* sql, 
                                RistrettoCallback callback, void* ctx) {
    Statement* stmt = parse_sql(sql);
    if (!stmt) {
        return RISTRETTO_PARSE_ERROR;
    }
    
    QueryPlan* plan = plan_statement(stmt, db);
    if (!plan) {
        statement_destroy(stmt);
        return RISTRETTO_ERROR;
    }
    
    QueryContext query_ctx = {
        .db = db,
        .pager = db->pager,
        .plan = plan,
        .callback = callback,
        .callback_ctx = ctx
    };
    
    RistrettoResult result = execute_plan(&query_ctx);
    
    plan_destroy(plan);
    statement_destroy(stmt);
    
    return result;
}

const char* ristretto_error_string(RistrettoResult result) {
    switch (result) {
        case RISTRETTO_OK:
            return "Success";
        case RISTRETTO_ERROR:
            return "General error";
        case RISTRETTO_NOMEM:
            return "Out of memory";
        case RISTRETTO_IO_ERROR:
            return "I/O error";
        case RISTRETTO_PARSE_ERROR:
            return "SQL parse error";
        case RISTRETTO_NOT_FOUND:
            return "Not found";
        case RISTRETTO_CONSTRAINT_ERROR:
            return "Constraint violation";
        default:
            return "Unknown error";
    }
}
