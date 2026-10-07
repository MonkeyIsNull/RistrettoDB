# RistrettoDB Python Bindings

ctypes bindings for [RistrettoDB](../../), a fast, embeddable, fixed-schema,
append-only, single-writer telemetry/analytics store written in C.

RistrettoDB is **not** a general-purpose SQL database: there is no query
language, JOINs, UPDATE/DELETE, or transactions. You declare a fixed schema,
append rows, and scan them back (filtering in Python).

## Requirements

- Python 3.6+
- The RistrettoDB shared library. Build it from the repo root:

  ```bash
  make dynamic    # lib/libristretto.so (Linux) / lib/libristretto.dylib (macOS)
  ```

`ristretto.py` locates the library relative to itself (repo `lib/`, alongside
the file, or the usual system paths).

## Quick start

```python
from ristretto import RistrettoTable, RistrettoValue, version

print("RistrettoDB", version())

with RistrettoTable.create(
        "events",
        "CREATE TABLE events (ts INTEGER, name TEXT(16), value REAL)") as table:
    table.append_row([RistrettoValue.integer(1672531200),
                      RistrettoValue.text("login"),
                      RistrettoValue.real(1.5)])
    # A NULL persists and round-trips as None.
    table.append_row([RistrettoValue.integer(1672531260),
                      RistrettoValue.null(),
                      RistrettoValue.real(2.0)])

    for row in table.scan():       # each row is a list of decoded values
        print(row)                 # e.g. [1672531200, 'login', 1.5]
```

Run the full demo:

```bash
python3 example.py
```

## API

Module functions:

- `version() -> str`, `version_number() -> int`

`RistrettoValue` constructors:

- `RistrettoValue.integer(int)`, `.real(float)`, `.text(str)`, `.null()`

`RistrettoTable`:

- `RistrettoTable.create(name, schema_sql)` — create (truncating any existing
  file). Tracks the column count parsed from the schema so `scan()` knows how
  many values to decode.
- `RistrettoTable.open(name, column_count=0)` — open an existing table; pass
  `column_count` so `scan()`/`select()` can decode rows.
- `append_row(values)` — append one row (one `RistrettoValue` per column).
- `get_row_count() -> int`
- `select(callback=None, column_count=None) -> list[list]` — scan every row
  (V2 has no WHERE clause). A NULL decodes to `None`, keyed off `is_null`.
- `scan(column_count=None) -> list[list]` — `select()` that always returns rows.
- `close()` (also a context manager).

## Notes / limitations

- **No query language.** Scan and filter in Python.
- **NULLs persist** and round-trip as `None`.
- **Single-writer**, advisory `flock` (a no-op on some network filesystems).
- **No WAL**: `close()` flushes durably, but a crash mid-run may lose rows since
  the last durable flush.
- A table named `foo` is stored at `data/foo.rdb` relative to the CWD.
