# RistrettoDB Programming Manual

RistrettoDB is a fast, embeddable, fixed-schema, append-only, single-writer
telemetry/analytics store written in C (the "Table V2" engine). This manual is
the complete reference for using it from C and from the Go / Python / Node
bindings.

RistrettoDB is **not** a general-purpose SQL database. There is no query
language at runtime — no `SELECT`, `JOIN`, `UPDATE`/`DELETE`, `WHERE`, or
transactions. The `CREATE TABLE` string you pass to `ristretto_table_create` is
**schema-declaration DDL only**: it is parsed solely to define the fixed-width
columns (names, types, widths), not executed as a SQL query — there is no SQL
engine behind it. You append rows and scan them, filtering in your own code.

## Table of Contents

1. [Introduction](#introduction)
2. [Building and Setup](#building-and-setup)
3. [Embedding Integration](#embedding-integration)
4. [The Table V2 API](#the-table-v2-api)
5. [Value Types](#value-types)
6. [Inserting Rows](#inserting-rows)
7. [Scanning Rows](#scanning-rows)
8. [NULL Handling](#null-handling)
9. [Durability and Concurrency](#durability-and-concurrency)
10. [Memory Management](#memory-management)
11. [Schema Design](#schema-design)
12. [Real-World Examples](#real-world-examples)
13. [Language Bindings](#language-bindings)
14. [Performance](#performance)
15. [Troubleshooting](#troubleshooting)
16. [Testing and Validation](#testing-and-validation)

## Introduction

RistrettoDB stores each table as a single `.rdb` file: a 1024-byte header
followed by fixed-width rows, memory-mapped for zero-copy access. Appends write
straight into the mapped region; scans walk the rows and hand each to a
callback.

Design properties:

- **Fixed schema** — up to 14 columns; INTEGER/REAL are 8 bytes, TEXT(n) is n
  bytes (≤ 255). Each row begins with a NULL bitmap.
- **Append-only** — no in-place update or delete.
- **Single-writer** — an advisory `flock` guards the file.
- **Persistent** — rows survive process restarts. NULLs persist and round-trip.
- **On-disk format v3** — older v1/v2 files are rejected cleanly on open.

- **RistrettoDB Version**: 0.3.0 (`ristretto_version()`)

## Building and Setup

### Prerequisites

- A C11 compiler (the Makefile defaults to `clang`)
- A POSIX system (Linux, macOS, BSD), little-endian, 64-bit

### Building

```bash
# Clone and build the libraries
git clone https://github.com/MonkeyIsNull/RistrettoDB && cd RistrettoDB
make libraries           # lib/libristretto.a and lib/libristretto.so

# Debug build with symbols
make debug

# Run the test suites
make test-v2 test-comprehensive test-stress test-golden
# or everything:
make test-all
```

### Version Information

```c
#include "ristretto.h"
#include <stdio.h>

int main(void) {
    printf("RistrettoDB %s (%d)\n",
           ristretto_version(),        // "0.3.0"
           ristretto_version_number()); // 3000  (MAJOR*1000000 + MINOR*1000 + PATCH)
    return 0;
}
```

### Build Outputs

```
lib/libristretto.a    # Static library (recommended for embedding)
lib/libristretto.so   # Dynamic library
embed/ristretto.h     # Single public header
embed/ristretto.c     # Generated single-file amalgamation
```

## Embedding Integration

### Link the library

Include `embed/ristretto.h` and link the static (or dynamic) library:

```bash
cc -O3 -Iembed myapp.c lib/libristretto.a -o myapp
```

### Single-file amalgamation

`scripts/embed.py` regenerates `embed/ristretto.c`. Two ways to use it:

```c
/* Option 1 — fully embedded, single translation unit: */
#define RISTRETTO_EMBEDDED
#include "ristretto.c"   /* note: .c, not .h */
```

```bash
# Option 2 — compile the amalgamation separately and link. The
# RISTRETTO_EMBEDDED define makes the unit provide the full implementation.
cc -std=c11 -DRISTRETTO_EMBEDDED -Iembed -c embed/ristretto.c -o ristretto.o
cc -std=c11 -Iembed myapp.c ristretto.o -o myapp
```

### The compatibility layer

`ristretto.h` exposes the prefixed API (`ristretto_table_*`,
`ristretto_value_*`, `RistrettoTable`, `RistrettoValue`) and also short aliases
(`table_*`, `value_*`, `Table`, `Value`) via a compatibility layer. Define
`RISTRETTO_NO_COMPATIBILITY_LAYER` before including the header to suppress the
aliases if they would clash with your own names.

## The Table V2 API

### Lifecycle

```c
// Create (truncating any existing file), stored at data/<name>.rdb:
RistrettoTable* ristretto_table_create(const char *name, const char *schema_sql);

// Open an existing table, resuming its rows:
RistrettoTable* ristretto_table_open(const char *name);

// Extended: choose the storage directory and open mode.
//   open_mode: RISTRETTO_CREATE_NEW (O_EXCL, fail if exists),
//              RISTRETTO_CREATE_OR_TRUNCATE (default of the 2-arg create),
//              RISTRETTO_OPEN_OR_CREATE.
RistrettoTable* ristretto_table_create_ex(const char *name, const char *schema_sql,
                                          const char *base_dir, int open_mode);
RistrettoTable* ristretto_table_open_ex(const char *name, const char *base_dir);

void ristretto_table_close(RistrettoTable *table);  // durable flush + unmap
```

### Core operations

```c
// Append one row. values[] must hold exactly column_count entries in order.
bool ristretto_table_append_row(RistrettoTable *table, const RistrettoValue *values);

// Count-checked append (recommended for bindings / untrusted callers).
bool ristretto_table_append_row_n(RistrettoTable *table, const RistrettoValue *values,
                                  uint32_t value_count);

// Scan every row, invoking callback once per row. V2 has no WHERE clause:
// scan and filter in your application.
bool ristretto_table_select(RistrettoTable *table,
                            void (*callback)(void *ctx, const RistrettoValue *row),
                            void *ctx);

size_t ristretto_table_get_row_count(RistrettoTable *table);
```

### Durability

```c
bool ristretto_table_flush(RistrettoTable *table);          // MS_ASYNC (fast)
bool ristretto_table_flush_durable(RistrettoTable *table);  // MS_SYNC + fsync
```

## Value Types

```c
RistrettoValue ristretto_value_integer(int64_t val);  // INTEGER (8 bytes)
RistrettoValue ristretto_value_real(double val);      // REAL (8 bytes)
RistrettoValue ristretto_value_text(const char *str); // TEXT(n), copied
RistrettoValue ristretto_value_null(void);            // NULL
void ristretto_value_destroy(RistrettoValue *value);  // frees TEXT storage
```

`ristretto_value_text` allocates a copy of the string; call
`ristretto_value_destroy` on any TEXT value you build once the append has
copied it into the row. A scanned row's Values are owned by the scan and freed
automatically after your callback returns (copy anything you need to keep).

The `RistrettoValue` struct:

```c
typedef struct {
    RistrettoColumnType type;     // RISTRETTO_COL_INTEGER / _REAL / _TEXT / _NULLABLE
    union {
        int64_t integer;
        double  real;
        struct { char *data; size_t length; } text;
    } value;
    bool is_null;                 // authoritative NULL flag (see below)
} RistrettoValue;
```

## Inserting Rows

```c
#include "ristretto.h"

int main(void) {
    RistrettoTable *t = ristretto_table_create("events",
        "CREATE TABLE events (timestamp INTEGER, user_id INTEGER, event TEXT(32))");

    RistrettoValue row[3];
    row[0] = ristretto_value_integer(1672531200);
    row[1] = ristretto_value_integer(12345);
    row[2] = ristretto_value_text("user_login");

    ristretto_table_append_row(t, row);

    ristretto_value_destroy(&row[2]);  // free the TEXT copy
    ristretto_table_close(t);
    return 0;
}
```

## Scanning Rows

The scan callback receives a pointer to an array of `column_count` Values, valid
only for the duration of the call:

```c
#include "ristretto.h"
#include <stdio.h>

static void on_row(void *ctx, const RistrettoValue *row) {
    long *count = (long *)ctx;
    (*count)++;
    printf("  ts=%lld user=%lld event=%s\n",
           (long long)row[0].value.integer,
           (long long)row[1].value.integer,
           row[2].is_null ? "(null)" : row[2].value.text.data);
}

void scan_all(RistrettoTable *t) {
    long count = 0;
    ristretto_table_select(t, on_row, &count);   // 3-arg: no WHERE clause
    printf("scanned %ld rows\n", count);
}
```

To filter, test fields inside your callback — there is no server-side `WHERE`.

## NULL Handling

Any column may hold a NULL. NULL-ness is stored in a per-row bitmap, so a NULL
written with `ristretto_value_null()` **persists and round-trips as NULL**.

`is_null` is the authoritative flag end-to-end. A round-tripped NULL reads back
with `row[i].is_null == true` and `row[i].type` equal to the column's declared
type (e.g. `RISTRETTO_COL_INTEGER`), **not** `RISTRETTO_COL_NULLABLE`. Always
test `is_null`, never `type == NULLABLE`:

```c
static void on_row(void *ctx, const RistrettoValue *row) {
    (void)ctx;
    if (row[0].is_null) {
        printf("id: NULL\n");
    } else {
        printf("id: %lld\n", (long long)row[0].value.integer);
    }
}
```

## Durability and Concurrency

- **Async flush during a run.** Appends flush with `msync(MS_ASYNC)` every
  `SYNC_INTERVAL_ROWS` rows / `SYNC_INTERVAL_MS` ms.
- **Durable on demand / on close.** `ristretto_table_flush_durable` does
  `msync(MS_SYNC)` + `fsync`; `ristretto_table_close` does the same before
  unmapping. There is **no WAL**: a crash mid-run can lose rows written since
  the last durable flush. Call `flush_durable` at checkpoints you care about.
- **Single writer.** `table_create`/`table_open` take an advisory `flock`, so a
  second process opening the same live table fails fast. The lock is advisory
  (cooperating processes only) and a **no-op on some network filesystems**
  (NFS/SMB). Open each table from one writer at a time.

## Memory Management

- Free every TEXT `RistrettoValue` you construct with
  `ristretto_value_destroy` after the append copies it.
- Do not free the Values handed to your scan callback — the scan owns them and
  frees them after the callback returns. Copy out anything you need to retain.
- `ristretto_table_close` flushes durably and unmaps; always close tables.

## Schema Design

- Declare columns with a `CREATE TABLE name (col TYPE, ...)` string. This string
  is schema-declaration DDL only — parsed solely to define the columns, never
  executed as a SQL query. Supported types: `INTEGER`, `REAL`, `TEXT(n)`
  (n ≤ 255; default 64 if unspecified).
- Up to 14 columns per table (`RISTRETTO_MAX_COLUMNS`).
- Choose TEXT widths deliberately: rows are fixed width, so a wide TEXT column
  costs that many bytes in every row.
- No `ALTER TABLE`: to change a schema, create a new table and copy rows.

## Real-World Examples

### IoT telemetry

```c
#include "ristretto.h"
#include <stdio.h>

int main(void) {
    RistrettoTable *sensors = ristretto_table_create("sensor_data",
        "CREATE TABLE sensor_data (ts INTEGER, device_id INTEGER, "
        "temperature REAL, humidity REAL, location TEXT(16))");

    for (int i = 0; i < 100000; i++) {
        RistrettoValue r[5];
        r[0] = ristretto_value_integer(1672531200 + i);
        r[1] = ristretto_value_integer(i % 100);
        r[2] = ristretto_value_real(20.0 + (i % 30));
        r[3] = ristretto_value_real(40.0 + (i % 40));
        r[4] = ristretto_value_text("rack-7");
        ristretto_table_append_row(sensors, r);
        ristretto_value_destroy(&r[4]);
    }

    ristretto_table_flush_durable(sensors);
    printf("rows: %zu\n", ristretto_table_get_row_count(sensors));
    ristretto_table_close(sensors);
    return 0;
}
```

### Security audit trail

```c
#include "ristretto.h"

void log_event(RistrettoTable *audit, int64_t ts, const char *user, const char *action) {
    RistrettoValue r[3];
    r[0] = ristretto_value_integer(ts);
    r[1] = ristretto_value_text(user);
    r[2] = ristretto_value_text(action);
    ristretto_table_append_row(audit, r);
    ristretto_value_destroy(&r[1]);
    ristretto_value_destroy(&r[2]);
    // Audit records should be durable immediately:
    ristretto_table_flush_durable(audit);
}
```

### Aggregating during a scan

```c
#include "ristretto.h"
#include <stdio.h>

typedef struct { long rows; double sum; } Agg;

static void acc(void *ctx, const RistrettoValue *row) {
    Agg *a = (Agg *)ctx;
    a->rows++;
    if (!row[2].is_null) a->sum += row[2].value.real;  // temperature column
}

double mean_temperature(RistrettoTable *t) {
    Agg a = {0, 0.0};
    ristretto_table_select(t, acc, &a);
    return a.rows ? a.sum / (double)a.rows : 0.0;
}
```

## Language Bindings

All three bindings expose the same Table V2 model: create/open, append rows,
scan them back (filtering in the host language), and NULLs round-trip as the
language's null value.

### Go (cgo)

```go
import "github.com/MonkeyIsNull/RistrettoDB/examples/go/ristretto"

t, _ := ristretto.CreateTable("events",
    "CREATE TABLE events (ts INTEGER, name TEXT(16), value REAL)")
defer t.Close()

_ = t.AppendRow([]ristretto.Value{
    ristretto.IntegerValue(1001),
    ristretto.TextValue("login"),
    ristretto.RealValue(1.5),
})

rows, _ := t.Scan()   // V2 has no WHERE; Scan returns all rows
for _, r := range rows {
    fmt.Println(r.Get("ts").Int(), r.Get("name").Text(), r.Get("value").Float())
}

fmt.Println(ristretto.Version())  // "0.3.0"
```

```bash
make static
cd examples/go && go build ./... && go test ./ristretto && go run ./cmd/example
```

### Python (ctypes)

```python
from ristretto import RistrettoTable, RistrettoValue, version

with RistrettoTable.create("events",
        "CREATE TABLE events (ts INTEGER, name TEXT(16), value REAL)") as t:
    t.append_row([RistrettoValue.integer(1001),
                  RistrettoValue.text("login"),
                  RistrettoValue.real(1.5)])
    for row in t.scan():          # [[1001, "login", 1.5]]
        print(row)

print(version())                  # "0.3.0"
```

```bash
make dynamic
python3 examples/python/example.py
```

### Node.js (koffi)

```javascript
const { RistrettoTable, RistrettoValue, version } = require('./ristretto');

const t = RistrettoTable.create('events',
  'CREATE TABLE events (ts INTEGER, name TEXT(16), value REAL)');
t.appendRow([RistrettoValue.integer(1001), RistrettoValue.text('login'), RistrettoValue.real(1.5)]);
const rows = t.select();          // [[1001, 'login', 1.5]]
t.close();

console.log(version());           // "0.3.0"
```

```bash
make dynamic
cd examples/nodejs && npm install && node example.js
```

## Performance

RistrettoDB is built for high append throughput: fixed-width rows, zero-copy
mmap writes, and no per-row parsing. The in-tree benchmark
(`benchmark/ultra_fast_benchmark.c`, run with `make benchmark`) measures
append throughput for 1,000,000 rows of `(id INTEGER, data TEXT(16))` under the
portable baseline (`-O3 -std=c11`, no `-march=native`) and prints rows/sec and
ns/row, plus a `malloc` baseline.

The exact number is flag- and machine-sensitive, so always report it with the
pinned configuration documented in the benchmark header / `benchmark/README.md`.
Tips for maximum throughput:

- Batch appends; call `flush_durable` only at the checkpoints you actually need.
- Keep TEXT columns as narrow as your data allows (rows are fixed width).
- Use `make SIMD=native` for host-tuned builds (not for shipped artifacts).

## Troubleshooting

| Symptom | Likely cause / fix |
| --- | --- |
| `table_create`/`open` returns NULL | Another process holds the advisory `flock`, or the path/base_dir is not writable. |
| `table_open` returns NULL on an old file | The file is format v1/v2; v3 rejects it. Recreate the table. |
| TEXT reads back truncated | TEXT(n) stores n-1 bytes + NUL; widen the column. |
| A NULL reads back as 0 / "" | You tested `type == NULLABLE`; test `is_null` instead. |
| Crash lost recent rows | Expected without a durable flush; call `flush_durable` at checkpoints. |
| Appends fail after many rows | Out of disk, or value count ≠ column count (use `append_row_n`). |

## Testing and Validation

```bash
# Core suites
make test-v2
make test-comprehensive      # validates manual claims
make test-stress
make test-golden             # on-disk format round-trip incl. NULLs + byte-pin
make test-all

# Memory safety (Linux CI builds the suites with ASan+UBSan)
clang -std=c11 -D_DEFAULT_SOURCE -Iinclude -Iembed -I. \
    -fsanitize=address,undefined -fno-sanitize-recover=all -g \
    src/table_v2.c src/ristretto_api.c src/version.c tests/test_golden_format.c \
    -o test_golden_san && ./test_golden_san
```

The comprehensive and golden suites validate the examples and the on-disk format
(including NULL persistence) on your platform.
