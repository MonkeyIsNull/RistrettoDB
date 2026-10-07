/*
** test_golden_format.c - Golden on-disk format round-trip test (format v3).
**
** Two layers:
**   1. PORTABLE (runs everywhere, unconditional): write rows mixing real values
**      and NULLs (including a NULL TEXT, which exercises the zero-union unpack
**      path), flush durably, close, reopen, and assert every field — including
**      is_null — round-trips exactly.
**   2. PLATFORM-PINNED byte assertions: header.version/magic/row_size and the
**      exact packed bytes of row 0. INTEGER/REAL are stored native-endian, so
**      the byte-pin is valid on the declared supported platform (little-endian,
**      64-bit: macOS/Linux on arm64/x86-64). On a hypothetical big-endian
**      runner the byte-pin is skipped (with a notice) rather than failing the
**      whole suite; the portable layer still runs.
**
** (No committed .rdb fixture: *.rdb is git-ignored, so the test regenerates the
** file deterministically each run.)
*/
#include "table_v2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define BASE_DIR "golden_test_data"
#define TABLE_NAME "golden"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else         { printf("  ok:   %s\n", msg); } \
} while (0)

static int is_little_endian(void) {
    uint32_t x = 1;
    return *(const uint8_t *)&x == 1;
}

int main(void) {
    printf("RistrettoDB Golden Format Test (v%d)\n", TABLE_VERSION);
    printf("====================================\n");

    /* Fresh table: INTEGER, REAL, TEXT(16). */
    const char *schema =
        "CREATE TABLE golden (a INTEGER, b REAL, c TEXT(16))";
    system("rm -rf " BASE_DIR);
    Table *t = table_create_ex(TABLE_NAME, schema, BASE_DIR, RDB_CREATE_OR_TRUNCATE);
    if (!t) { printf("FATAL: table_create_ex failed\n"); return 1; }

    /* row 0: all real values */
    Value r0[3] = { value_integer(42), value_real(3.5), value_text("hello") };
    /* row 1: NULL integer */
    Value r1[3] = { value_null(), value_real(2.0), value_text("world") };
    /* row 2: NULL text (exercises the zero-union free path) */
    Value r2[3] = { value_integer(7), value_real(1.25), value_null() };
    /* row 3: NULL real */
    Value r3[3] = { value_integer(-9), value_null(), value_text("x") };

    if (!table_append_row(t, r0) || !table_append_row(t, r1) ||
        !table_append_row(t, r2) || !table_append_row(t, r3)) {
        printf("FATAL: append failed\n"); return 1;
    }
    value_destroy(&r0[2]); value_destroy(&r1[2]); value_destroy(&r3[2]);

    if (!table_flush_durable(t)) { printf("FATAL: flush_durable failed\n"); return 1; }

    uint32_t expect_row_size = NULL_BITMAP_BYTES + 8 + 8 + 16;
    table_close(t);

    /* --- Reopen and assert values round-trip (portable) --- */
    t = table_open_ex(TABLE_NAME, BASE_DIR);
    if (!t) { printf("FATAL: reopen failed (v3 file rejected?)\n"); return 1; }

    CHECK(table_get_row_count(t) == 4, "row count == 4");

    Value row[3];
    uint8_t *base = t->mapped_ptr + TABLE_HEADER_SIZE;
    uint32_t rs = t->header->row_size;

    /* row 0 */
    table_unpack_row(t, base + 0 * rs, row);
    CHECK(!row[0].is_null && row[0].value.integer == 42, "row0.a == 42");
    CHECK(!row[1].is_null && row[1].value.real == 3.5,  "row0.b == 3.5");
    CHECK(!row[2].is_null && strcmp(row[2].value.text.data, "hello") == 0, "row0.c == \"hello\"");
    for (int i = 0; i < 3; i++) value_destroy(&row[i]);

    /* row 1: NULL integer */
    table_unpack_row(t, base + 1 * rs, row);
    CHECK(row[0].is_null, "row1.a is NULL");
    CHECK(!row[1].is_null && row[1].value.real == 2.0, "row1.b == 2.0");
    CHECK(!row[2].is_null && strcmp(row[2].value.text.data, "world") == 0, "row1.c == \"world\"");
    for (int i = 0; i < 3; i++) value_destroy(&row[i]);

    /* row 2: NULL text */
    table_unpack_row(t, base + 2 * rs, row);
    CHECK(!row[0].is_null && row[0].value.integer == 7, "row2.a == 7");
    CHECK(!row[1].is_null && row[1].value.real == 1.25, "row2.b == 1.25");
    CHECK(row[2].is_null && row[2].value.text.data == NULL, "row2.c is NULL (data==NULL)");
    for (int i = 0; i < 3; i++) value_destroy(&row[i]);

    /* row 3: NULL real */
    table_unpack_row(t, base + 3 * rs, row);
    CHECK(!row[0].is_null && row[0].value.integer == -9, "row3.a == -9");
    CHECK(row[1].is_null, "row3.b is NULL");
    CHECK(!row[2].is_null && strcmp(row[2].value.text.data, "x") == 0, "row3.c == \"x\"");
    for (int i = 0; i < 3; i++) value_destroy(&row[i]);

    /* --- Header assertions (portable) --- */
    CHECK(memcmp(t->header->magic, TABLE_MAGIC, 8) == 0, "magic == RSTRDB");
    CHECK(t->header->version == TABLE_VERSION, "header.version == 3");
    CHECK(t->header->row_size == expect_row_size, "row_size == bitmap+8+8+16");
    CHECK(t->header->column_count == 3, "column_count == 3");

    /* --- Platform-pinned byte assertions (little-endian, 64-bit) --- */
    if (is_little_endian() && sizeof(void *) == 8) {
        uint8_t *row0 = base;
        /* bitmap: no NULLs in row 0 */
        int bitmap_zero = 1;
        for (int i = 0; i < NULL_BITMAP_BYTES; i++) if (row0[i] != 0) bitmap_zero = 0;
        CHECK(bitmap_zero, "byte-pin: row0 NULL bitmap is all zero");

        int64_t a = 42; double b = 3.5;
        CHECK(memcmp(row0 + NULL_BITMAP_BYTES, &a, 8) == 0,
              "byte-pin: row0.a bytes == int64 42 (LE native)");
        CHECK(memcmp(row0 + NULL_BITMAP_BYTES + 8, &b, 8) == 0,
              "byte-pin: row0.b bytes == double 3.5 (native)");
        CHECK(memcmp(row0 + NULL_BITMAP_BYTES + 16, "hello", 6) == 0,
              "byte-pin: row0.c bytes == \"hello\\0\"");

        /* row 2 bitmap: column 2 (TEXT) is NULL -> bit 2 set in byte 0 */
        uint8_t *row2 = base + 2 * rs;
        CHECK((row2[0] & (1u << 2)) != 0, "byte-pin: row2 bitmap bit 2 set (NULL text)");
    } else {
        printf("  note: non-LE/non-64-bit platform; skipping byte-pin assertions\n");
    }

    table_close(t);
    system("rm -rf " BASE_DIR);

    printf("====================================\n");
    if (failures == 0) {
        printf("SUCCESS: golden format round-trip passed.\n");
        return 0;
    }
    printf("FAILURE: %d golden checks failed.\n", failures);
    return 1;
}
