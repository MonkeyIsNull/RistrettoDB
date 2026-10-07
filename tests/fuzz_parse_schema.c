/*
** fuzz_parse_schema.c - libFuzzer target over table_parse_schema.
**
** table_parse_schema parses a CREATE TABLE schema string. This harness feeds
** it arbitrary bytes under ASan+UBSan to catch memory-safety bugs. The parser
** expects a NUL-terminated C string, so the raw (data,size) buffer is copied
** into a malloc(size+1) with a trailing '\0' before the call — otherwise the
** fuzzer would flag out-of-bounds reads it created itself.
**
** Build + run (ubuntu-latest, LLVM clang; Apple clang ships no libFuzzer):
**   clang -std=c11 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all \
**       -Iinclude tests/fuzz_parse_schema.c src/table_v2.c -o fuzz_parse_schema
**   ./fuzz_parse_schema -runs=100000 -max_total_time=30
*/
#include "table_v2.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char *s = (char *)malloc(size + 1);
    if (!s) return 0;
    memcpy(s, data, size);
    s[size] = '\0';

    ColumnDesc columns[MAX_COLUMNS];
    uint32_t column_count = 0, row_size = 0;
    (void)table_parse_schema(s, columns, &column_count, &row_size);

    free(s);
    return 0;
}
