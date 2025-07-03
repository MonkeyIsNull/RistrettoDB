# RistrettoDB

A tiny, blazingly fast, embeddable SQL engine written in C.

## Features

- Minimal SQL subset: CREATE TABLE, INSERT, SELECT, UPDATE, DELETE
- Memory-mapped storage for zero-copy I/O
- Hard-coded execution pipelines (no VM overhead)
- SIMD-optimized filtering
- Fixed-width row format
- B+Tree indexing

## Building

```bash
make
```

For debug build:
```bash
make debug
```

## Usage

```bash
./ristretto
```

## Testing

```bash
make test
```

## Performance Goals

Designed to outperform SQLite in narrowly optimized scenarios through:
- Direct memory-mapped I/O
- SIMD vectorization for WHERE clauses
- Columnar data layout options
- Zero-copy data access
- Minimal abstraction overhead