/*
** test_where.c - Regression tests for WHERE-clause evaluation (issue #4).
**
** Before the fix, execute_select emitted every row and ignored the scan
** filter, so WHERE returned the wrong data. These tests cover the scalar
** path, an AND case, the >100-row SIMD path (and its agreement with a scalar
** control on the same predicate), the TEXT NULL-guard in storage_value_compare,
** and that an unsupported LIKE surfaces an error instead of a wrong row set.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        printf("Running test: %s ... ", #name); \
        tests_run++; \
        if (test_##name()) { printf("PASS\n"); tests_passed++; } \
        else { printf("FAIL\n"); tests_failed++; } \
    } while(0)

#define REQUIRE(condition, message) \
    do { \
        if (!(condition)) { printf("FAIL: %s\n", message); return false; } \
    } while(0)

static void count_cb(void* ctx, int n_cols, char** values, char** col_names) {
    (void)n_cols; (void)values; (void)col_names;
    (*(int*)ctx)++;
}

// Run SELECT and return the number of rows emitted (-1 on query error).
static int count_rows(RistrettoDB* db, const char* sql) {
    int count = 0;
    RistrettoResult r = ristretto_query(db, sql, count_cb, &count);
    if (r != RISTRETTO_OK) return -1;
    return count;
}

static bool insert_ints(RistrettoDB* db, const char* table, int from, int to) {
    char sql[128];
    for (int i = from; i <= to; i++) {
        snprintf(sql, sizeof(sql), "INSERT INTO %s VALUES (%d)", table, i);
        if (ristretto_exec(db, sql) != RISTRETTO_OK) return false;
    }
    return true;
}

// (a) Basic scalar comparison: ids 1..10, WHERE id > 7 -> 8,9,10.
bool test_where_basic_gt(void) {
    RistrettoDB* db = ristretto_open("where_basic.db");
    REQUIRE(db, "open failed");
    REQUIRE(ristretto_exec(db, "CREATE TABLE wa (id INTEGER)") == RISTRETTO_OK, "create failed");
    REQUIRE(insert_ints(db, "wa", 1, 10), "insert failed");

    REQUIRE(count_rows(db, "SELECT * FROM wa") == 10, "unfiltered count wrong");
    REQUIRE(count_rows(db, "SELECT * FROM wa WHERE id > 7") == 3, "id>7 should be 3");
    REQUIRE(count_rows(db, "SELECT * FROM wa WHERE id = 5") == 1, "id=5 should be 1");
    REQUIRE(count_rows(db, "SELECT * FROM wa WHERE id < 3") == 2, "id<3 should be 2");
    ristretto_close(db);
    return true;
}

// (b) AND of two comparisons: WHERE id > 3 AND id < 7 -> 4,5,6.
bool test_where_and(void) {
    RistrettoDB* db = ristretto_open("where_and.db");
    REQUIRE(db, "open failed");
    REQUIRE(ristretto_exec(db, "CREATE TABLE wb (id INTEGER)") == RISTRETTO_OK, "create failed");
    REQUIRE(insert_ints(db, "wb", 1, 10), "insert failed");

    REQUIRE(count_rows(db, "SELECT * FROM wb WHERE id > 3 AND id < 7") == 3, "AND range should be 3");
    REQUIRE(count_rows(db, "SELECT * FROM wb WHERE id < 3 OR id > 8") == 4, "OR should be 4 (1,2,9,10)");
    ristretto_close(db);
    return true;
}

// (c) >100-row SIMD path, with a <=100-row scalar control on the same
// predicate: both must agree with the known expected counts.
bool test_where_simd_scalar_agreement(void) {
    RistrettoDB* db = ristretto_open("where_simd.db");
    REQUIRE(db, "open failed");

    // Scalar control: 50 rows (<=100 -> scalar path).
    REQUIRE(ristretto_exec(db, "CREATE TABLE wsmall (id INTEGER)") == RISTRETTO_OK, "create small failed");
    REQUIRE(insert_ints(db, "wsmall", 1, 50), "insert small failed");

    // SIMD: 150 rows (>100 -> execute_select_simd for INTEGER EQ/GT/LT).
    REQUIRE(ristretto_exec(db, "CREATE TABLE wbig (id INTEGER)") == RISTRETTO_OK, "create big failed");
    REQUIRE(insert_ints(db, "wbig", 1, 150), "insert big failed");

    // GT
    REQUIRE(count_rows(db, "SELECT * FROM wsmall WHERE id > 40") == 10, "scalar id>40 should be 10");
    REQUIRE(count_rows(db, "SELECT * FROM wbig   WHERE id > 40") == 110, "simd id>40 should be 110");
    // EQ
    REQUIRE(count_rows(db, "SELECT * FROM wsmall WHERE id = 25") == 1, "scalar id=25 should be 1");
    REQUIRE(count_rows(db, "SELECT * FROM wbig   WHERE id = 125") == 1, "simd id=125 should be 1");
    // LT
    REQUIRE(count_rows(db, "SELECT * FROM wsmall WHERE id < 10") == 9, "scalar id<10 should be 9");
    REQUIRE(count_rows(db, "SELECT * FROM wbig   WHERE id < 10") == 9, "simd id<10 should be 9");
    // Full agreement: a predicate matching the same absolute ids in both tables.
    REQUIRE(count_rows(db, "SELECT * FROM wsmall WHERE id > 45") == 5, "scalar id>45 should be 5");
    REQUIRE(count_rows(db, "SELECT * FROM wbig   WHERE id > 145") == 5, "simd id>145 should be 5");
    ristretto_close(db);
    return true;
}

// (d) TEXT-column predicate: exercises the scalar storage_value_compare TEXT
// path (SIMD is INTEGER-only) and its NULL-data guard.
bool test_where_text(void) {
    RistrettoDB* db = ristretto_open("where_text.db");
    REQUIRE(db, "open failed");
    REQUIRE(ristretto_exec(db, "CREATE TABLE wt (id INTEGER, name TEXT)") == RISTRETTO_OK, "create failed");
    REQUIRE(ristretto_exec(db, "INSERT INTO wt VALUES (1, 'alpha')") == RISTRETTO_OK, "insert 1 failed");
    REQUIRE(ristretto_exec(db, "INSERT INTO wt VALUES (2, 'beta')")  == RISTRETTO_OK, "insert 2 failed");
    REQUIRE(ristretto_exec(db, "INSERT INTO wt VALUES (3, 'alpha')") == RISTRETTO_OK, "insert 3 failed");

    REQUIRE(count_rows(db, "SELECT * FROM wt WHERE name = 'alpha'") == 2, "name='alpha' should be 2");
    REQUIRE(count_rows(db, "SELECT * FROM wt WHERE name = 'gamma'") == 0, "name='gamma' should be 0");
    ristretto_close(db);
    return true;
}

// (e) LIKE must surface an explicit error, not silently return a wrong set.
bool test_where_like_unsupported(void) {
    RistrettoDB* db = ristretto_open("where_like.db");
    REQUIRE(db, "open failed");
    REQUIRE(ristretto_exec(db, "CREATE TABLE wl (id INTEGER, name TEXT)") == RISTRETTO_OK, "create failed");
    REQUIRE(ristretto_exec(db, "INSERT INTO wl VALUES (1, 'alpha')") == RISTRETTO_OK, "insert failed");
    REQUIRE(ristretto_exec(db, "INSERT INTO wl VALUES (2, 'beta')")  == RISTRETTO_OK, "insert failed");

    // count_rows returns -1 when the query returns a non-OK result.
    int n = 0;
    RistrettoResult r = ristretto_query(db, "SELECT * FROM wl WHERE name LIKE 'a%'", count_cb, &n);
    REQUIRE(r != RISTRETTO_OK, "LIKE should surface an error");
    REQUIRE(n == 0, "LIKE must not emit rows");
    ristretto_close(db);
    return true;
}

int main(void) {
    printf("=== WHERE clause tests (issue #4) ===\n");
    system("rm -f where_*.db");

    TEST(where_basic_gt);
    TEST(where_and);
    TEST(where_simd_scalar_agreement);
    TEST(where_text);
    TEST(where_like_unsupported);

    system("rm -f where_*.db");

    printf("\n===============================\n");
    printf("Tests passed: %d/%d\n", tests_passed, tests_run);
    return tests_failed == 0 ? 0 : 1;
}
