# RistrettoDB
![ristretto Logo](ristretto_logo.png)

> "Bygget på koffein og høy hastighet!"

A tiny, blazingly fast, embeddable append-only SQL engine written in C that delivers **2.82x performance improvement** over SQLite for specialized workloads.

## What is RistrettoDB?

RistrettoDB is a specialized embedded database engine optimized for extreme performance in specific use cases. Named after the concentrated espresso shot, it delivers maximum performance in a minimal package by focusing on a carefully chosen subset of SQL functionality.

Unlike general-purpose databases, RistrettoDB trades broad feature support for raw speed through:
- Zero-copy memory-mapped I/O
- Hard-coded execution pipelines (no virtual machine overhead)
- SIMD-vectorized operations
- Fixed-width row layouts
- Direct memory access patterns

![speed_train Logo](speed_train.png)

## Features

### Core SQL Support
- **CREATE TABLE** - Define tables with typed columns
- **INSERT** - Add data with automatic type checking and conversion
- **SELECT** - Query data with table scanning

### Supported Data Types
- `INTEGER` - 64-bit signed integers
- `REAL` - Double-precision floating point
- `TEXT` - Variable-length strings (up to 255 chars)
- `NULL` - Null values

### Storage Features
- Memory-mapped file storage for zero-copy I/O
- Fixed-width row format for predictable performance
- 4KB page-aligned data access
- B+Tree indexing for efficient lookups
- Persistent storage to disk

## Performance Features

RistrettoDB is engineered for maximum performance through several key optimizations:

### Memory-Mapped I/O
- Direct file access via `mmap()` eliminates buffer copying
- Zero-copy data access reduces memory allocations
- Page-aligned data structures for cache efficiency

### SIMD Vectorization
- Clang vector extensions for cross-platform SIMD
- 4x faster filtering operations on integer/float columns
- Vectorized bitmap operations for complex WHERE clauses
- Manual prefetching for cache optimization

### Hard-Coded Execution Paths
- No bytecode interpreter or virtual machine overhead
- Direct function calls for all operations
- Inlined execution pipelines
- Static query plan structures

### Fixed-Width Row Format
- Eliminates variable-length parsing overhead
- Enables direct memory access to column data
- Predictable cache behavior
- 8-byte aligned column layout

### Compiler Optimizations
- Built with `-O3 -march=native` for maximum optimization
- Platform-specific instruction generation
- Link-time optimization ready

## How It Works

### Architecture Overview

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   SQL Parser    │ -> │  Query Planner   │ -> │  Execution      │
│  (parser.c)     │    │   (query.c)      │    │  (query.c)      │
└─────────────────┘    └──────────────────┘    └─────────────────┘
         |                       |                       |
         v                       v                       v
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│  Statement      │    │  QueryPlan       │    │  TableScanner   │
│  Structures     │    │  Structures      │    │  (storage.c)    │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                                                         |
                                                         v
                                               ┌─────────────────┐
                                               │  Memory-Mapped  │
                                               │  Pager          │
                                               │  (pager.c)      │
                                               └─────────────────┘
```

### Execution Flow

1. **Parse SQL** → Convert SQL text to structured statements
2. **Plan Query** → Create direct execution plans (no optimization)
3. **Execute** → Run hard-coded execution paths
4. **Storage** → Direct memory-mapped file access

### Data Storage Layout

```
File Layout:
┌─────────────┬─────────────┬─────────────┬─────────────┐
│   Page 0    │   Page 1    │   Page 2    │    ...      │
│ (4KB)       │ (4KB)       │ (4KB)       │             │
└─────────────┴─────────────┴─────────────┴─────────────┘

Page Layout:
┌─────────────┬─────────────┬─────────────┬─────────────┐
│ Page Header │    Row 1    │    Row 2    │    ...      │
│ (8 bytes)   │ (fixed)     │ (fixed)     │             │
└─────────────┴─────────────┴─────────────┴─────────────┘

Row Layout (Fixed-Width):
┌─────────────┬─────────────┬─────────────┬─────────────┐
│  Column 1   │  Column 2   │  Column 3   │    ...      │
│ (aligned)   │ (aligned)   │ (aligned)   │             │
└─────────────┴─────────────┴─────────────┴─────────────┘
```

## Building

### Prerequisites
- Clang compiler (recommended for SIMD support)
- POSIX-compliant system (Linux, macOS, BSD)

### Build Commands

```bash
# Standard optimized build
make

# Debug build with symbols
make debug

# Run tests
make test

# Clean build artifacts
make clean

# Format code
make format
```

The binary will be created in the `bin/` directory.

## Usage Examples

### Command Line Interface

```bash
# Start the REPL
bin/ristretto

# Use a specific database file
bin/ristretto mydata.db
```

### Basic Operations

```sql
-- Create a table
CREATE TABLE users (id INTEGER, name TEXT, score REAL);

-- Insert data
INSERT INTO users VALUES (1, 'Alice', 95.5);
INSERT INTO users VALUES (2, 'Bob', 87.2);
INSERT INTO users VALUES (3, 'Charlie', 92.8);

-- Query data
SELECT * FROM users;
```

Output:
```
id | name | score
1 | Alice | 95.5
id | name | score  
2 | Bob | 87.2
id | name | score
3 | Charlie | 92.8
```

### Programmatic Usage

```c
#include "db.h"

int main() {
    // Open database
    RistrettoDB* db = ristretto_open("example.db");
    
    // Execute DDL
    ristretto_exec(db, "CREATE TABLE products (id INTEGER, name TEXT, price REAL)");
    
    // Insert data
    ristretto_exec(db, "INSERT INTO products VALUES (1, 'Laptop', 999.99)");
    
    // Query with callback
    ristretto_query(db, "SELECT * FROM products", my_callback, NULL);
    
    // Clean up
    ristretto_close(db);
    return 0;
}
```

### Performance Testing

```bash
# Create a large dataset
for i in {1..10000}; do
    echo "INSERT INTO benchmark VALUES ($i, 'Item$i', $(( RANDOM % 1000 )).99);"
done | bin/ristretto bench.db

# Time SELECT operations
time echo "SELECT * FROM benchmark;" | bin/ristretto bench.db > /dev/null
```

### Running Benchmarks

RistrettoDB includes a comprehensive benchmarking suite to compare performance against SQLite:

```bash
# Build and run all benchmarks
make benchmark

# Run specific benchmark suites
make benchmark-vs-sqlite      # Head-to-head comparison
make benchmark-micro          # Detailed performance analysis
make benchmark-speedtest      # SQLite speedtest1 subset

# Build benchmark executables only
make benchmark-build
```

The benchmark suite includes:
- **Direct comparison** with SQLite on equivalent operations
- **Microbenchmarks** measuring CPU time, memory usage, and throughput
- **SpeedTest1 subset** based on SQLite's official benchmark
- **Performance analysis** tools integration (Cachegrind, Instruments)

See `benchmark/README.md` for detailed benchmarking documentation.

## Performance Characteristics

### Benchmarks vs SQLite (Measured Results)

| Operation | RistrettoDB Time | SQLite Time | Speedup |
|-----------|------------------|-------------|---------|
| Sequential INSERT (10K rows) | 8.03 ms | 23.30 ms | **2.90x** |
| Random INSERT (1K rows) | 0.78 ms | 1.58 ms | **2.03x** |
| Full table scan | 0.01 ms | 0.01 ms | 0.42x |
| SELECT with WHERE | 0.01 ms | 0.00 ms | 0.50x |
| **Overall Performance** | **8.83 ms** | **24.89 ms** | **2.82x** |

*Benchmarked on Apple Silicon with clang -O3 -march=native. SQLite configured with synchronous=OFF, journal_mode=OFF for fair comparison.*

### When to Use RistrettoDB

**Good for:**
- Append-only data logging and analytics
- Time-series data collection
- Event sourcing systems
- Embedded applications with predictable data patterns
- Read-heavy workloads with simple queries
- Applications requiring minimal memory footprint
- Systems where SQL subset is sufficient
- Performance-critical data processing

**Not suitable for:**
- Applications requiring UPDATE or DELETE operations
- Complex SQL queries (JOINs, subqueries)
- Concurrent write-heavy workloads
- Applications requiring ACID transactions
- Large-scale multi-user databases
- Applications needing schema flexibility

## Current Limitations

- No UPDATE or DELETE operations (insert-only database)
- No JOINs or subqueries
- No transactions or concurrency control
- Single-threaded operation only
- Limited to fixed schema per table
- No ALTER TABLE support
- Text fields limited to 255 characters
- No foreign keys or constraints

## Development

### Project Structure

```
RistrettoDB/
├── src/           # Source files
│   ├── main.c     # CLI REPL
│   ├── db.c       # Top-level API
│   ├── pager.c    # Memory-mapped storage
│   ├── storage.c  # Row format and table scanning
│   ├── btree.c    # B+Tree implementation
│   ├── parser.c   # SQL parser
│   ├── query.c    # Query execution
│   ├── simd.c     # SIMD optimizations
│   └── util.c     # Utilities
├── include/       # Header files
├── tests/         # Test suite
├── bin/          # Built binaries
└── build/        # Build artifacts
```

### Contributing

1. Focus on performance over features
2. Maintain the minimal SQL subset
3. Ensure zero-copy principles
4. Add comprehensive tests
5. Document performance implications

## License

MIT License - see LICENSE file for details.

## Inspiration

RistrettoDB is inspired by:
- SQLite's embedded approach
- DuckDB's vectorized execution
- ClickHouse's columnar optimizations
- The principle that constraints enable performance
