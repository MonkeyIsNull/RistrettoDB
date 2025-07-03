# RistrettoDB Benchmarking Suite

This directory contains a comprehensive benchmarking suite to compare RistrettoDB against SQLite and analyze RistrettoDB's performance characteristics.

## Available Benchmarks

### 1. `benchmark.c` - Head-to-Head Comparison
Directly compares RistrettoDB against SQLite on equivalent operations:
- Sequential INSERTs
- Random INSERTs  
- Full table scans
- SELECT with WHERE clauses

**Features:**
- Uses SQLite's in-memory mode for fair comparison
- Disables SQLite journaling and synchronous writes
- Measures wall-clock time with high precision
- Calculates speedup ratios

### 2. `microbench.c` - Detailed Performance Analysis
Analyzes RistrettoDB's performance characteristics in detail:
- CPU time (user/system) measurement
- Memory usage tracking (RSS)
- Operations per second calculation
- Isolated operation testing

**Metrics Tracked:**
- Wall clock time
- User CPU time
- System CPU time
- Peak resident set size (memory)
- Throughput (operations/second)

### 3. `speedtest_subset.c` - SQLite SpeedTest1 Adaptation
Based on SQLite's official `speedtest1.c` benchmark, adapted for RistrettoDB's feature set:
- Multiple INSERT patterns (sequential, indexed, random)
- Various SELECT patterns
- Industry-standard test methodology

## Building and Running

### Prerequisites
```bash
# Install SQLite development headers
# macOS:
brew install sqlite3

# Ubuntu/Debian:
sudo apt-get install libsqlite3-dev

# RHEL/CentOS:
sudo yum install sqlite-devel
```

### Build All Benchmarks
```bash
cd benchmark
make
```

### Run Complete Benchmark Suite
```bash
make run-all
```

### Run Individual Benchmarks
```bash
# Head-to-head comparison
make run-benchmark

# Detailed performance analysis
make run-microbench

# SpeedTest1 subset
make run-speedtest
```

## Performance Analysis Tools

### CPU Profiling (macOS)
```bash
make profile-benchmark
```
Uses Instruments to create detailed CPU usage profiles.

### Cache Analysis (Linux/macOS)
```bash
make cachegrind-benchmark
```
Uses Valgrind's Cachegrind to analyze cache hit/miss patterns.

### Memory Analysis
```bash
make memory-benchmark
```
Uses Valgrind to detect memory leaks and analyze allocation patterns.

## Understanding Results

### Benchmark Output Format
```
Test Name                  | SQLite Time | Ristretto Time | Speedup
---------------------------|-------------|----------------|--------
Sequential INSERT          |    1.23s    |     0.45s     |  2.73x
```

### Key Metrics

**Speedup Ratio:**
- `> 1.0x`: RistrettoDB is faster
- `< 1.0x`: SQLite is faster
- `~1.0x`: Roughly equivalent performance

**Operations/Second:**
- Higher is better
- Compare similar operations across databases
- Consider feature parity when interpreting

### Expected Performance Characteristics

**RistrettoDB Advantages:**
- Sequential INSERTs (no journaling overhead)
- Full table scans (memory-mapped access)
- Simple queries (no query planner overhead)
- Memory efficiency (fixed-width rows)

**SQLite Advantages:**
- Complex queries (mature optimizer)
- Concurrent access (better locking)
- Feature completeness (transactions, etc.)
- Random access patterns (B+tree maturity)

## Interpreting Results

### Factors Affecting Performance

**Hardware Dependencies:**
- CPU architecture (SIMD instruction availability)
- Memory bandwidth (mmap performance)
- Storage type (SSD vs HDD for disk-based tests)
- Cache sizes (L1/L2/L3 impact)

**Compiler Optimizations:**
- `-march=native` enables CPU-specific optimizations
- `-O3` provides aggressive optimization
- Profile-guided optimization (PGO) could improve results further

**Operating System:**
- Page cache behavior
- Memory allocator efficiency
- System call overhead

### Benchmark Limitations

**RistrettoDB Limitations:**
- No UPDATE/DELETE support (affects some comparisons)
- No complex WHERE clauses yet
- No JOIN operations
- No transactions

**Fair Comparison Notes:**
- SQLite configured with minimal safety (no journaling)
- In-memory mode used to minimize I/O differences
- Both use default settings unless noted

## Adding Custom Benchmarks

### Example Custom Benchmark
```c
static void my_custom_test(RistrettoDB *db, int iterations) {
    // Your custom test here
    for (int i = 0; i < iterations; i++) {
        // Test operations
    }
}

// Add to benchmark array:
{"My Test", my_sqlite_version, my_custom_test, 1000}
```

### Benchmark Best Practices

1. **Warm-up runs**: Run small tests first to warm caches
2. **Multiple iterations**: Average results across multiple runs
3. **Statistical significance**: Consider variance in results
4. **Isolation**: Run benchmarks on idle systems
5. **Documentation**: Note any configuration changes

## Continuous Performance Monitoring

### Automated Benchmarking
```bash
#!/bin/bash
# Example CI/CD performance monitoring
cd benchmark
make clean
make benchmarks

# Run benchmarks and save results
make run-all > results_$(date +%Y%m%d).txt

# Alert on performance regressions
# (implement comparison logic)
```

### Performance Regression Detection
- Compare results across commits
- Set acceptable performance thresholds  
- Alert on significant slowdowns
- Track performance trends over time

## Troubleshooting

### Common Issues

**Build Errors:**
```bash
# Missing SQLite headers
sudo apt-get install libsqlite3-dev

# Clang not found
export CC=gcc  # Use GCC instead
```

**Runtime Errors:**
```bash
# Permission denied on file creation
chmod +w /tmp  # Or run in writable directory

# Segmentation faults
gdb ./bin/benchmark  # Debug with GDB
```

**Unexpected Results:**
- Check system load (`top`, `htop`)
- Verify compiler flags match between builds
- Run multiple times and average results
- Check for background processes affecting performance

## Contributing

When adding new benchmarks:
1. Follow existing naming conventions
2. Document what the benchmark measures
3. Include both SQLite and RistrettoDB implementations
4. Add appropriate error handling
5. Update this README with new benchmark descriptions