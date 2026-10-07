# RistrettoDB Node.js Bindings

[koffi](https://koffi.dev) FFI bindings for [RistrettoDB](../../), a fast,
embeddable, fixed-schema, append-only, single-writer telemetry/analytics store
written in C.

RistrettoDB is **not** a general-purpose SQL database: there is no query
language, JOINs, UPDATE/DELETE, or transactions. You declare a fixed schema,
append rows, and scan them back (filtering in JS).

## Requirements

- Node.js 18+
- `koffi` (prebuilt FFI, no native build step): `npm install`
- The RistrettoDB shared library. Build it from the repo root:

  ```bash
  make dynamic    # lib/libristretto.so (Linux) / lib/libristretto.dylib (macOS)
  ```

`ristretto.js` locates the library relative to itself (repo `lib/`, alongside
the file, or the usual system paths).

## Quick start

```javascript
const { RistrettoTable, RistrettoValue, version } = require('./ristretto');

console.log('RistrettoDB', version());

const table = RistrettoTable.create('events',
  'CREATE TABLE events (ts INTEGER, name TEXT(16), value REAL)');

table.appendRow([RistrettoValue.integer(1672531200),
                 RistrettoValue.text('login'),
                 RistrettoValue.real(1.5)]);
// A NULL persists and round-trips as null.
table.appendRow([RistrettoValue.integer(1672531260),
                 RistrettoValue.null(),
                 RistrettoValue.real(2.0)]);

const rows = table.select();   // [[1672531200,'login',1.5], [1672531260,null,2.0]]
console.log(rows);
table.close();
```

Run the full demo:

```bash
npm install
node example.js
```

## API

Module exports: `RistrettoTable`, `RistrettoValue`, `RistrettoError`,
`RistrettoResult`, `RistrettoColumnType`, `version()`, `versionNumber()`,
`libraryPath`.

`RistrettoValue` constructors:

- `RistrettoValue.integer(n)`, `.real(n)`, `.text(s)`, `.null()`

`RistrettoTable`:

- `RistrettoTable.create(name, schemaSql)` — create (truncating any existing
  file). Learns the column count from the schema so `select()` can decode rows.
- `RistrettoTable.open(name, columnCount = 0)` — open an existing table; pass
  `columnCount` so `select()` can decode rows.
- `appendRow(values)` — append one row (one `RistrettoValue` per column).
- `getRowCount()`
- `select(callback, columnCount = this.columnCount)` — scan every row (V2 has no
  WHERE clause). Each row is an array of decoded values; a NULL decodes to
  `null`, keyed off `is_null`. Returns the array of per-row arrays.
- `close()`

## Notes / limitations

- **No query language.** Scan and filter in JS.
- **NULLs persist** and round-trip as `null`.
- **Single-writer**, advisory `flock` (a no-op on some network filesystems).
- **No WAL**: `close()` flushes durably, but a crash mid-run may lose rows since
  the last durable flush.
- A table named `foo` is stored at `data/foo.rdb` relative to the CWD.
