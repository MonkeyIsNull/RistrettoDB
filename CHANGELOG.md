# Changelog

All notable changes to RistrettoDB are documented in this file.

The format is loosely based on [Keep a Changelog](https://keepachangelog.com/),
and the project aims to follow semantic versioning once it reaches 1.0.

## [0.3.0] — V2-only pivot

This release makes RistrettoDB a single-engine project: the **Table V2** engine
(fast, embeddable, fixed-schema, append-only, single-writer) is now the whole
library. The earlier in-memory "Original SQL" prototype has been removed.

### Changed (breaking)

- **Removed the Original SQL engine.** The `ristretto_open` / `ristretto_exec`
  / `ristretto_query` / `ristretto_close` API and the SQL `RistrettoDB` /
  `RistrettoResult` / statement types are gone, along with the SQL CLI. The
  public header (`embed/ristretto.h`) and the single-file amalgamation
  (`embed/ristretto.c`) are now V2-only. Use the `ristretto_table_*` /
  `ristretto_value_*` API.
- **On-disk format v2 → v3 (BREAKING).** Rows now begin with a per-row NULL
  bitmap, so the row layout changed. Existing v1/v2 `.rdb` files are **rejected,
  not migrated** — the magic/version check fails cleanly on open. Recreate your
  tables with this release.
- **`table_select` lost its `where_clause` parameter.** V2 has no WHERE clause;
  the new signature is `table_select(table, callback, ctx)`. Scan and filter in
  your application.
- **Version reset to 0.3.0** (from the earlier, misleading `2.0.0`) — honest
  "early but real". `RISTRETTO_VERSION_NUMBER` is `3000`
  (`MAJOR*1000000 + MINOR*1000 + PATCH`).

### Added

- **NULL persistence.** A NULL written to any column now persists and
  round-trips as NULL (keyed off `is_null`), instead of reading back as 0 / 0.0
  / "". Implemented with a fixed-width NULL bitmap at the start of each row.
- **Golden on-disk round-trip test** (`tests/test_golden_format.c`): writes rows
  mixing real values and NULLs, reopens, and asserts every field (including
  NULLs and the exact packed bytes of row 0 on little-endian 64-bit platforms).
- **ASan + UBSan CI job** (ubuntu-latest) building and running the V2 suites
  with `-fsanitize=address,undefined`.
- **Optional libFuzzer harness** for the schema parser (`tests/fuzz_parse_schema.c`).
- `LICENSE` (MIT), this `CHANGELOG.md`, and `SECURITY.md`.

### Fixed

- **Misaligned scalar load/store in row pack/unpack.** INTEGER/REAL fields are
  now read/written with `memcpy` instead of type-punned pointer casts, which is
  UB and can SIGBUS on strict-alignment targets (and is required now that the
  NULL bitmap shifts every column off 8-byte alignment).
- NULL TEXT unpack fully initialises the Value union, preventing a free of
  uninitialised memory.

### Removed

- The Original SQL engine sources, SQL CLI, SQL test suites, SQL benchmarks, and
  the SQL surface of the Go / Python / Node bindings and C examples.
- The "general-purpose SQL" and "2.8× / 4.57× / 4.6M-rows-over-SQLite"
  marketing claims (there is no SQL engine to compare against).

### Platform support

Supported: POSIX, little-endian, 64-bit (macOS / Linux on arm64 / x86-64).
Windows, big-endian, and 32-bit are explicitly unsupported (the format is
native-endian and mmap/`flock`-based).
