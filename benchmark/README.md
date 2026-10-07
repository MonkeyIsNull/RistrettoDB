# RistrettoDB Benchmark

This directory contains the RistrettoDB write-throughput benchmark.

RistrettoDB is an append-only store with no SQL engine, so there is no
general-purpose database to compare against. The benchmark measures what the
engine actually does — appending fixed-width rows — and reports the result with
a pinned, reproducible configuration.

## The benchmark: `ultra_fast_benchmark.c`

Appends rows to a Table V2 table and reports throughput (rows/sec) and latency
(ns/row), plus a `malloc` baseline for context.

### Pinned configuration

The throughput number is flag- and machine-sensitive, so it is only meaningful
alongside the configuration it was produced with:

| Parameter      | Value                                                      |
| -------------- | ---------------------------------------------------------- |
| Compiler flags | `-O3 -std=c11` (portable baseline, **no** `-march=native`) |
| Rows           | 1,000,000 (`BENCHMARK_ROWS`)                               |
| Schema         | `(id INTEGER, data TEXT(16))`                              |
| Machine class  | modern arm64 / x86-64 laptop or server, warm page cache    |

Always quote this configuration with any number you report. (Host-tuned builds
with `SIMD=native` and different machines will produce different figures.)

## Running

```bash
# From the repo root:
make benchmark

# Or directly:
make -C benchmark run-ultra-fast
```

## Optional SQLite contrast (off by default)

An optional contrast against SQLite's in-memory `INSERT` path is available
behind a compile flag. It is **off by default** and never required by CI
(sqlite3 is not a build dependency):

```bash
# Requires sqlite3 development headers:
#   macOS:        brew install sqlite3
#   Debian/Ubuntu: sudo apt-get install libsqlite3-dev
make -C benchmark run-ultra-fast WITH_SQLITE=1
```

This is a contrast between two *different* workloads (RistrettoDB's append path
vs. SQLite's in-memory row inserts), not an apples-to-apples speedup claim.
RistrettoDB does not implement SQL.

## Portability note

The benchmark Makefile uses the portable baseline by default (no
`-march=native`, which would produce binaries that `SIGILL` on a different
microarchitecture). Opt into host tuning with `make SIMD=native`.
