# RistrettoDB Programming Manual

A comprehensive guide to building high-performance applications with RistrettoDB's dual-engine architecture.

## Table of Contents

1. [Introduction](#introduction)
2. [Building and Setup](#building-and-setup)
3. [Embedding Integration](#embedding-integration)
4. [Language Bindings](#language-bindings)
5. [Choosing the Right API](#choosing-the-right-api)
6. [Original SQL API](#original-sql-api)
7. [Table V2 Ultra-Fast API](#table-v2-ultra-fast-api)
8. [Real-World Examples](#real-world-examples)
9. [Performance Optimization](#performance-optimization)
10. [Best Practices](#best-practices)
11. [Production Deployment](#production-deployment)
12. [Testing and Validation](#testing-and-validation)
13. [Troubleshooting](#troubleshooting)

---

## Introduction

RistrettoDB provides **two complementary database engines** optimized for different use cases:

### **Original SQL API** - General Purpose
- **Performance**: 2.8x faster than SQLite
- **Interface**: Standard SQL strings (`INSERT`, `SELECT`, etc.)
- **Use Cases**: General embedded SQL, mixed workloads
- **File**: `#include "db.h"`

### **Table V2 API** - Ultra-Fast Writes
- **Performance**: 4.6 million rows/sec (4.57x faster than SQLite)
- **Interface**: Direct C API with typed values
- **Use Cases**: High-speed logging, telemetry, analytics ingestion
- **File**: `#include "table_v2.h"`

**Key Decision**: Use Table V2 for write-heavy workloads requiring maximum speed. Use Original for general SQL compatibility.

> **Validation**: All examples in this manual are validated by a comprehensive test suite. Run `make test-comprehensive` to verify everything works correctly on your system.

---

## Building and Setup

### Prerequisites

RistrettoDB is a standalone program and library.
If you wish to run benchmark tests you will need to install sqlite3

```bash
# macOS
brew install sqlite3

# Ubuntu/Debian  
sudo apt-get install libsqlite3-dev build-essential

# RHEL/CentOS
sudo yum install sqlite-devel gcc
```

### Building RistrettoDB

```bash
# Clone and build
git clone <repository-url>
cd RistrettoDB

# Standard optimized build
make clean && make

# Debug build with symbols
make debug

# Run tests
make test          # Original API tests
make test-v2       # Table V2 tests
make test-comprehensive    # Validates ALL manual examples
make test-stress   # High-volume performance tests
make test-all      # Run all test suites

# Run benchmarks
make benchmark-ultra-fast    # V2 performance test
make benchmark-vs-sqlite     # Original vs SQLite
```

### Version Information

- **RistrettoDB Version**: 2.0
- **Table V2 Format Version**: 1
- **Compiler Requirements**: C11, Clang/GCC with `-march=native`
- **Dependencies**: SQLite3 (for benchmarking only)
- **Platforms**: Linux, macOS, BSD (POSIX-compliant)

### Build Outputs

```bash
bin/ristretto              # Original SQL CLI
bin/test_table_v2          # V2 API tests
bin/ultra_fast_benchmark   # Performance benchmarks
```

---

## Embedding Integration

RistrettoDB is designed for **seamless embedding** in C/C++ applications with a single header file approach, similar to SQLite but optimized for high-speed writes.

### Single Header Design

```c
#include "ristretto.h"  // Everything you need in one header

// Complete API access:
// - Version information
// - Original SQL API (ristretto_* functions)
// - Table V2 Ultra-Fast API (table_* and value_* functions)
// - Error handling and result codes
```

### Library Integration Steps

#### Step 1: Build the Library

```bash
# Build static and dynamic libraries
make lib

# Verify build outputs
ls -la lib/
# libristretto.a     42KB    # Static library (recommended)
# libristretto.so    56KB    # Dynamic library
```

#### Step 2: Install Headers and Libraries

```bash
# System-wide installation
sudo cp embed/ristretto.h /usr/local/include/
sudo cp lib/libristretto.a /usr/local/lib/
sudo cp lib/libristretto.so /usr/local/lib/

# Or project-local installation
mkdir -p myproject/deps/ristretto/
cp embed/ristretto.h myproject/deps/ristretto/
cp lib/libristretto.a myproject/deps/ristretto/
```

#### Step 3: Link in Your Project

```bash
# Static linking (recommended for embedding)
gcc -O3 myapp.c -lristretto -o myapp

# Dynamic linking
gcc -O3 myapp.c -lristretto -o myapp
LD_LIBRARY_PATH=/path/to/lib ./myapp

# With custom paths
gcc -O3 -I./deps/ristretto/ myapp.c ./deps/ristretto/libristretto.a -o myapp
```

### Complete Embedding Example

```c
#include "ristretto.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Application logger using both APIs
typedef struct {
    RistrettoDB* sql_db;      // For structured queries
    Table* fast_log;          // For high-speed logging
} AppLogger;

AppLogger* logger_create(const char* app_name) {
    AppLogger* logger = malloc(sizeof(AppLogger));
    if (!logger) return NULL;
    
    // Initialize SQL database for configuration and metadata
    char sql_path[256];
    snprintf(sql_path, sizeof(sql_path), "%s_config.db", app_name);
    logger->sql_db = ristretto_open(sql_path);
    
    if (!logger->sql_db) {
        free(logger);
        return NULL;
    }
    
    // Create configuration table
    ristretto_exec(logger->sql_db, 
        "CREATE TABLE app_config (key TEXT, value TEXT, timestamp INTEGER)");
    
    // Initialize ultra-fast logging table
    char log_schema[512];
    snprintf(log_schema, sizeof(log_schema),
        "CREATE TABLE %s_logs ("
        "timestamp INTEGER, "
        "level INTEGER, "      // 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
        "thread_id INTEGER, "
        "component TEXT(32), "
        "message TEXT(256)"
        ")", app_name);
    
    logger->fast_log = table_create("app_logs", log_schema);
    if (!logger->fast_log) {
        ristretto_close(logger->sql_db);
        free(logger);
        return NULL;
    }
    
    return logger;
}

// High-speed logging function
void logger_log(AppLogger* logger, int level, int thread_id, 
                const char* component, const char* message) {
    if (!logger || !component || !message) return;
    
    Value values[5];
    values[0] = value_integer(time(NULL));
    values[1] = value_integer(level);
    values[2] = value_integer(thread_id);
    values[3] = value_text(component);
    values[4] = value_text(message);
    
    table_append_row(logger->fast_log, values);
    
    // Clean up text values
    value_destroy(&values[3]);
    value_destroy(&values[4]);
}

// Configuration management using SQL API
void logger_set_config(AppLogger* logger, const char* key, const char* value) {
    if (!logger || !key || !value) return;
    
    char sql[512];
    snprintf(sql, sizeof(sql),
        "INSERT INTO app_config VALUES ('%s', '%s', %ld)",
        key, value, time(NULL));
    
    ristretto_exec(logger->sql_db, sql);
}

void logger_destroy(AppLogger* logger) {
    if (logger) {
        ristretto_close(logger->sql_db);
        table_close(logger->fast_log);
        free(logger);
    }
}

// Example application using embedded RistrettoDB
int main() {
    printf("RistrettoDB %s - Embedded Application Example\n", ristretto_version());
    
    // Initialize application logger
    AppLogger* logger = logger_create("MyApp");
    if (!logger) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }
    
    // Set application configuration
    logger_set_config(logger, "log_level", "INFO");
    logger_set_config(logger, "max_connections", "100");
    
    // High-speed application logging
    logger_log(logger, 1, 1, "main", "Application started");
    logger_log(logger, 1, 1, "auth", "Authentication system initialized");
    logger_log(logger, 2, 2, "network", "High latency detected");
    
    // Simulate high-frequency logging
    clock_t start = clock();
    for (int i = 0; i < 10000; i++) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Processing request %d", i);
        logger_log(logger, 1, i % 4 + 1, "worker", msg);
    }
    double elapsed = ((double)(clock() - start)) / CLOCKS_PER_SEC;
    
    printf("Logged 10,000 messages in %.3f seconds\n", elapsed);
    printf("Throughput: %.0f messages/second\n", 10000.0 / elapsed);
    
    logger_log(logger, 1, 1, "main", "Application shutdown");
    logger_destroy(logger);
    
    printf("Application completed successfully\n");
    return 0;
}
```

### Build System Integration

#### Makefile Integration

```makefile
# Project Makefile with RistrettoDB
CC = gcc
CFLAGS = -O3 -Wall -Wextra
RISTRETTO_DIR = deps/ristretto

# Include RistrettoDB
INCLUDES = -I$(RISTRETTO_DIR)
LIBS = $(RISTRETTO_DIR)/libristretto.a

# Your application
SOURCES = main.c app_logic.c utils.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = myapp

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Download and build RistrettoDB dependency
$(RISTRETTO_DIR)/libristretto.a:
	git clone https://github.com/yourorg/RistrettoDB $(RISTRETTO_DIR)
	cd $(RISTRETTO_DIR) && make lib
	cp $(RISTRETTO_DIR)/embed/ristretto.h $(RISTRETTO_DIR)/
	cp $(RISTRETTO_DIR)/lib/libristretto.a $(RISTRETTO_DIR)/

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: clean
```

#### CMake Integration

```cmake
# CMakeLists.txt with RistrettoDB
cmake_minimum_required(VERSION 3.10)
project(MyApp)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native")

# Find or build RistrettoDB
find_library(RISTRETTO_LIB ristretto PATHS ${CMAKE_SOURCE_DIR}/deps/ristretto)
find_path(RISTRETTO_INCLUDE ristretto.h PATHS ${CMAKE_SOURCE_DIR}/deps/ristretto/embed)

if(NOT RISTRETTO_LIB OR NOT RISTRETTO_INCLUDE)
    message(STATUS "Building RistrettoDB from source")
    
    # Download RistrettoDB
    include(ExternalProject)
    ExternalProject_Add(
        ristretto_ext
        GIT_REPOSITORY https://github.com/yourorg/RistrettoDB
        PREFIX ${CMAKE_BINARY_DIR}/ristretto
        CONFIGURE_COMMAND ""
        BUILD_COMMAND make lib
        BUILD_IN_SOURCE 1
        INSTALL_COMMAND ""
    )
    
    set(RISTRETTO_DIR ${CMAKE_BINARY_DIR}/ristretto/src/ristretto_ext)
    set(RISTRETTO_LIB ${RISTRETTO_DIR}/lib/libristretto.a)
    set(RISTRETTO_INCLUDE ${RISTRETTO_DIR})
endif()

# Your application
add_executable(myapp main.c app_logic.c utils.c)

target_include_directories(myapp PRIVATE ${RISTRETTO_INCLUDE})
target_link_libraries(myapp ${RISTRETTO_LIB})

if(TARGET ristretto_ext)
    add_dependencies(myapp ristretto_ext)
endif()
```

### Memory Management Guidelines

```c
// CRITICAL: Always destroy text values to prevent memory leaks
void correct_text_value_usage() {
    Value text_val = value_text("Some text");
    
    // Use the value...
    table_append_row(table, &text_val);
    
    // REQUIRED: Clean up allocated memory
    value_destroy(&text_val);
}

// WRONG: Memory leak
void incorrect_text_value_usage() {
    Value text_val = value_text("Some text");
    table_append_row(table, &text_val);
    // BUG: Forgot value_destroy(&text_val) - memory leaked!
}

// Helper for safe batch operations
typedef struct {
    Value* values;
    int capacity;
    int count;
} ValueBatch;

ValueBatch* value_batch_create(int capacity) {
    ValueBatch* batch = malloc(sizeof(ValueBatch));
    if (!batch) return NULL;
    
    batch->values = calloc(capacity, sizeof(Value));
    if (!batch->values) {
        free(batch);
        return NULL;
    }
    
    batch->capacity = capacity;
    batch->count = 0;
    return batch;
}

void value_batch_destroy(ValueBatch* batch) {
    if (!batch) return;
    
    for (int i = 0; i < batch->count; i++) {
        value_destroy(&batch->values[i]);
    }
    
    free(batch->values);
    free(batch);
}
```

---

## Language Bindings

RistrettoDB provides **complete, production-ready bindings** for Python, Node.js, and Go. Each binding is fully implemented with working examples, comprehensive documentation, and real-world use cases.

### Overview

**All language bindings provide**:
- **Dual API Support**: Both Original SQL API (2.8x faster) and Table V2 Ultra-Fast API (4.57x faster)
- **Complete Implementation**: Full working code, not prototypes
- **Production Ready**: Error handling, resource management, thread safety
- **Working Examples**: Real-world use cases with complete code
- **Easy Integration**: Simple installation and setup process

**Location**: All bindings are in the `examples/` directory:
```
examples/
├── python/          # Python bindings (ctypes-based)
├── nodejs/          # Node.js bindings (ffi-napi-based)  
└── go/              # Go bindings (cgo-based)
```

**Quick Comparison**:

| Language | Implementation | Dependencies | Best For |
|----------|---------------|--------------|----------|
| **Python** | ctypes | Zero (standard library) | Data science, ML, analytics |
| **Node.js** | ffi-napi | ffi-napi, ref-napi | Web APIs, microservices, real-time |
| **Go** | cgo | Zero (standard library) | Cloud-native, microservices, systems |

### Python Bindings

**Location**: `examples/python/`  
**Implementation**: ctypes-based, zero dependencies  
**Performance**: 2.8x faster (SQL) / 4.57x faster (Table V2) than SQLite

#### Quick Start

```bash
# 1. Build RistrettoDB library
cd ../../ && make lib

# 2. Run Python example
cd examples/python
python3 example.py

# 3. Copy bindings to your project
cp examples/python/ristretto.py your_project/
```

#### Basic Usage

```python
from ristretto import RistrettoDB, RistrettoTable, RistrettoValue

# Original SQL API - General Purpose (2.8x faster than SQLite)
with RistrettoDB("myapp.db") as db:
    db.exec("CREATE TABLE users (id INTEGER, name TEXT, score REAL)")
    db.exec("INSERT INTO users VALUES (1, 'Alice', 95.5)")
    
    results = db.query("SELECT * FROM users WHERE score > 90")
    for row in results:
        print(f"User: {row['name']}, Score: {row['score']}")

# Table V2 API - Ultra-Fast Writes (4.57x faster than SQLite)
with RistrettoTable.create("events", 
                         "CREATE TABLE events (timestamp INTEGER, event TEXT(32))") as table:
    
    values = [
        RistrettoValue.integer(1672531200),
        RistrettoValue.text("user_login")
    ]
    table.append_row(values)
    
    print(f"Total events: {table.get_row_count()}")
```

#### API Reference

**RistrettoDB (Original SQL API)**

```python
# Constructor & Context Manager
db = RistrettoDB(filename: str)
with RistrettoDB("mydb.db") as db:  # Recommended - auto-close

# Methods
db.exec(sql: str) -> None                    # Execute DDL/DML statements
db.query(sql: str) -> List[Dict[str, str]]   # Execute SELECT queries
db.close() -> None                           # Close database connection

# Static Methods
RistrettoDB.version() -> str                 # Get library version
```

**RistrettoTable (Table V2 Ultra-Fast API)**

```python
# Creation/Opening & Context Manager
table = RistrettoTable.create(name: str, schema_sql: str)
table = RistrettoTable.open(name: str)
with RistrettoTable.create("logs", "CREATE TABLE logs (...)") as table:

# Methods
table.append_row(values: List[RistrettoValue]) -> bool
table.get_row_count() -> int
table.close() -> None
```

**RistrettoValue (Value Types)**

```python
# Factory Methods
RistrettoValue.integer(value: int) -> RistrettoValue
RistrettoValue.real(value: float) -> RistrettoValue
RistrettoValue.text(value: str) -> RistrettoValue
RistrettoValue.null() -> RistrettoValue

# Properties
value.type: RistrettoColumnType    # Column type (INTEGER, REAL, TEXT, NULLABLE)
value.value: Any                   # The actual value
value.is_null: bool               # Whether value is null
```

#### Error Handling

```python
from ristretto import RistrettoError, RistrettoResult

try:
    with RistrettoDB("mydb.db") as db:
        db.exec("INVALID SQL")
except RistrettoError as e:
    print(f"Database error: {e}")
    print(f"Error code: {e.result_code}")  # RistrettoResult enum
```

#### Installation & Requirements

**System Requirements**
- Python 3.6+
- RistrettoDB library built (`make lib`)
- POSIX-compliant system (Linux, macOS, BSD)

**Installation Steps**
```bash
# 1. Build RistrettoDB
cd /path/to/RistrettoDB && make lib

# 2. Copy Python bindings
cp examples/python/ristretto.py your_project/

# 3. Import and use
# In your Python code:
from ristretto import RistrettoDB
```

**Zero Dependencies**: Uses only Python standard library with ctypes

#### Use Cases

**Perfect for**:
- **Web Applications**: High-speed session storage, user analytics
- **Data Analytics**: Real-time event ingestion, time-series data
- **Machine Learning**: Feature storage, training data preparation
- **IoT Systems**: Sensor data collection, device telemetry
- **Security**: Audit trails, tamper-evident logging
- **Gaming**: Player statistics, match analytics
- **Finance**: Trading data, transaction logging
- **Mobile Apps**: Offline data sync, local analytics

---

### Node.js Bindings

**Location**: `examples/nodejs/`  
**Implementation**: ffi-napi based, minimal dependencies  
**Performance**: 2.8x faster (SQL) / 4.57x faster (Table V2) than SQLite

#### Quick Start

```bash
# 1. Build RistrettoDB library
cd ../../ && make lib

# 2. Install Node.js dependencies
cd examples/nodejs
npm install

# 3. Run Node.js example
node example.js

# 4. Copy bindings to your project
cp examples/nodejs/ristretto.js your_project/
cp examples/nodejs/package.json your_project/  # for dependencies
```

#### Basic Usage

```javascript
const { RistrettoDB, RistrettoTable, RistrettoValue } = require('./ristretto');

// Original SQL API - General Purpose (2.8x faster than SQLite)
const db = new RistrettoDB('myapp.db');
db.exec('CREATE TABLE users (id INTEGER, name TEXT, score REAL)');
db.exec("INSERT INTO users VALUES (1, 'Alice', 95.5)");

const results = db.query('SELECT * FROM users WHERE score > 90');
results.forEach(row => {
  console.log(`User: ${row.name}, Score: ${row.score}`);
});
db.close();

// Table V2 API - Ultra-Fast Writes (4.57x faster than SQLite)
const table = RistrettoTable.create('events', 
  'CREATE TABLE events (timestamp INTEGER, event TEXT(32))');

table.appendRow([
  RistrettoValue.integer(Date.now()),
  RistrettoValue.text('user_login')
]);

console.log(`Total events: ${table.getRowCount()}`);
table.close();
```

#### API Reference

**RistrettoDB (Original SQL API)**

```javascript
// Constructor
const db = new RistrettoDB(filename)

// Methods
db.exec(sql)                         // Execute DDL/DML statements
db.query(sql, callback?)             // Execute SELECT queries, returns array of objects
db.close()                          // Close database connection

// Static Methods
RistrettoDB.version()               // Get library version
```

**RistrettoTable (Table V2 Ultra-Fast API)**

```javascript
// Creation/Opening
const table = RistrettoTable.create(name, schemaSql)
const table = RistrettoTable.open(name)

// Methods
table.appendRow(values)             // High-speed row insertion
table.getRowCount()                 // Get total number of rows
table.close()                       // Close table
```

**RistrettoValue (Value Types)**

```javascript
// Factory Methods
RistrettoValue.integer(value)       // Create integer value
RistrettoValue.real(value)          // Create real/float value  
RistrettoValue.text(value)          // Create text value
RistrettoValue.null()               // Create null value

// Properties
value.type                          // Column type (INTEGER, REAL, TEXT, NULLABLE)
value.value                         // The actual value
value.isNull                        // Whether value is null
```

#### Installation & Requirements

**System Requirements**
- Node.js 12.0.0+
- Dependencies: ffi-napi, ref-napi
- RistrettoDB library built (`make lib`)
- POSIX-compliant system (Linux, macOS, BSD)

**Dependencies Installation**
```bash
# Install required Node.js packages
npm install ffi-napi ref-napi

# Or copy from examples
cp examples/nodejs/package.json your_project/
npm install
```

#### Use Cases

**Perfect for**:
- **Web Applications**: Express.js APIs, session storage, user analytics
- **Real-time Analytics**: Live dashboards, event tracking, metrics collection
- **IoT Data Processing**: Sensor data ingestion, device telemetry
- **Gaming Backends**: Player statistics, match data, leaderboards
- **Security Logging**: Audit trails, access logs, intrusion detection
- **FinTech Applications**: Transaction logging, trading data, compliance
- **Mobile Backends**: User data sync, offline-first applications
- **Serverless Functions**: AWS Lambda, Vercel, Netlify edge functions

---

### Go Bindings

**Location**: `examples/go/`  
**Implementation**: cgo-based, native Go integration  
**Performance**: 2.8x faster (SQL) / 4.57x faster (Table V2) than SQLite

#### Quick Start

```bash
# 1. Build RistrettoDB library
cd ../../ && make lib

# 2. Set up environment for cgo
export CGO_LDFLAGS="-L../../lib"
export LD_LIBRARY_PATH="../../lib:$LD_LIBRARY_PATH"

# 3. Run Go example
cd examples/go
go run example.go

# 4. Copy bindings to your project
cp examples/go/ristretto.go your_project/
cp examples/go/go.mod your_project/  # optional
```

#### Basic Usage

```go
package main

import (
    "fmt"
    "log"
    "./ristretto" // Adjust path as needed
)

func main() {
    // Original SQL API - General Purpose (2.8x faster than SQLite)
    db, err := ristretto.Open("myapp.db")
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    err = db.Exec("CREATE TABLE users (id INTEGER, name TEXT, score REAL)")
    if err != nil {
        log.Fatal(err)
    }

    err = db.Exec("INSERT INTO users VALUES (1, 'Alice', 95.5)")
    if err != nil {
        log.Fatal(err)
    }

    results, err := db.Query("SELECT * FROM users WHERE score > 90")
    if err != nil {
        log.Fatal(err)
    }

    for _, row := range results {
        fmt.Printf("User: %s, Score: %s\n", row["name"], row["score"])
    }

    // Table V2 API - Ultra-Fast Writes (4.57x faster than SQLite)  
    table, err := ristretto.CreateTable("events", 
        "CREATE TABLE events (timestamp INTEGER, event TEXT(32))")
    if err != nil {
        log.Fatal(err)
    }
    defer table.Close()

    values := []ristretto.Value{
        ristretto.IntegerValue(time.Now().Unix()),
        ristretto.TextValue("user_login"),
    }

    err = table.AppendRow(values)
    if err != nil {
        log.Fatal(err)
    }

    fmt.Printf("Total events: %d\n", table.GetRowCount())
}
```

#### API Reference

**Package Functions**

```go
func Version() string                    // Get library version
func VersionNumber() int                 // Get version number
func Open(filename string) (*DB, error) // Open database
func CreateTable(name, schema string) (*Table, error) // Create table
func OpenTable(name string) (*Table, error)           // Open table
```

**Value Types**

```go
func IntegerValue(val int64) Value       // Create integer value
func RealValue(val float64) Value        // Create real value
func TextValue(val string) Value         // Create text value
func NullValue() Value                   // Create null value
```

**DB (Original SQL API)**

```go
type DB struct { /* ... */ }

// Methods
func (db *DB) Close() error
func (db *DB) Exec(sql string) error
func (db *DB) Query(sql string) ([]QueryResult, error)

// QueryResult is map[string]string representing a row
type QueryResult map[string]string
```

**Table (Table V2 Ultra-Fast API)**

```go
type Table struct { /* ... */ }

// Methods
func (t *Table) Close() error
func (t *Table) AppendRow(values []Value) error
func (t *Table) GetRowCount() int64
func (t *Table) Name() string
```

#### Installation & Requirements

**System Requirements**
- Go 1.19+
- CGO enabled (default)
- RistrettoDB library built (`make lib`)
- POSIX-compliant system (Linux, macOS, BSD)

**Build Configuration**

**Environment Variables**
```bash
export CGO_LDFLAGS="-L/path/to/ristrettodb/lib"
export LD_LIBRARY_PATH="/path/to/ristrettodb/lib:$LD_LIBRARY_PATH"
```

**Build Commands**
```bash
# Simple build
go build example.go

# Build with library path
go build -ldflags "-L../../lib" example.go

# Cross-compilation (ensure library is available for target)
GOOS=linux GOARCH=amd64 go build example.go
```

#### Use Cases

**Perfect for**:
- **Web Services**: High-performance REST APIs, GraphQL backends
- **Microservices**: Service mesh data, inter-service communication
- **Analytics**: Real-time data processing, time-series analysis
- **ML/AI Systems**: Feature stores, training data pipelines
- **IoT Platforms**: Device telemetry, sensor data aggregation
- **Security Tools**: Log analysis, threat detection, audit systems
- **Game Backends**: Player stats, match data, leaderboards
- **FinTech**: Transaction processing, trading systems, risk analysis
- **DevOps Tools**: Monitoring systems, CI/CD pipelines, metrics
- **Mobile Backends**: User data sync, analytics, push notifications

---

### Working Examples

Each language binding includes **complete working examples**:

**Python Examples**:
- `examples/python/example.py` - Complete demonstration
- `examples/python/README.md` - Full documentation
- Real-world use cases: IoT data, web analytics, security logging

**Node.js Examples**:
- `examples/nodejs/example.js` - Complete demonstration  
- `examples/nodejs/README.md` - Full documentation
- Real-world use cases: Express.js APIs, WebSocket servers, analytics

**Go Examples**:
- `examples/go/example.go` - Complete demonstration
- `examples/go/README.md` - Full documentation  
- Real-world use cases: Web APIs, microservices, IoT platforms

### Testing the Bindings

**Run all language binding examples**:
```bash
# Build RistrettoDB first
make lib

# Test Python bindings
cd examples/python && python3 example.py

# Test Node.js bindings  
cd examples/nodejs && npm install && node example.js

# Test Go bindings
cd examples/go && go run example.go
```

**Validate examples with comprehensive tests**:
```bash
# This validates all programming manual examples work correctly
make test-comprehensive
```

### Integration Paths

**Single Header Embedding** (recommended for C/C++):
```c
#include "embed/ristretto.h"  // Everything you need in one header
```

**Language Bindings** (recommended for Python/Node.js/Go):
```bash
# Copy language-specific bindings
cp examples/python/ristretto.py your_project/    # Python
cp examples/nodejs/ristretto.js your_project/    # Node.js  
cp examples/go/ristretto.go your_project/        # Go
```

**Library Linking** (for other languages):
```bash
# Use the shared library with any FFI-capable language
lib/libristretto.so    # Dynamic library
lib/libristretto.a     # Static library
embed/ristretto.h      # C header for bindings
```

---

## Choosing the Right API

### Decision Matrix

| Requirement | Original API | Table V2 API |
|-------------|--------------|--------------|
| **SQL compatibility** | YES: Full SQL parsing | NO: Direct API only |
| **Write performance** | ~1M rows/sec | FAST: 4.6M rows/sec |
| **Query flexibility** | YES: SQL WHERE, etc. | FAST: Table scan only |
| **Memory usage** | Variable | FAST: Fixed, predictable |
| **Schema changes** | YES: Dynamic | NO: Fixed at creation |
| **Transactions** | NO: Not supported | NO: Not supported |
| **File format** | B+Tree pages | FAST: Append-only |

### Use Case Guidelines

**Choose Table V2 API when:**
- Writing >100K rows/second
- Fixed, known schema
- Logging, telemetry, analytics
- Memory/performance critical
- Embedded systems

**Choose Original API when:**
- Need SQL string compatibility
- Variable schemas
- Mixed read/write workloads
- Prototyping/development
- Existing SQL knowledge

---

## Original SQL API

### Basic Usage

```c
#include "db.h"
#include <stdio.h>

int main() {
    // Open database
    RistrettoDB* db = ristretto_open("myapp.db");
    if (!db) {
        fprintf(stderr, "Failed to open database\n");
        return 1;
    }
    
    // Create table
    RistrettoResult result = ristretto_exec(db, 
        "CREATE TABLE users (id INTEGER, name TEXT, email TEXT)");
    if (result != RISTRETTO_OK) {
        fprintf(stderr, "Failed to create table: %s\n", 
                ristretto_error_string(result));
        ristretto_close(db);
        return 1;
    }
    
    printf("Database created successfully\n");
    ristretto_close(db);
    return 0;
}
```

### Inserting Data

```c
#include "db.h"

// Single insert
int insert_user(RistrettoDB* db, int id, const char* name, const char* email) {
    char sql[512];
    snprintf(sql, sizeof(sql), 
        "INSERT INTO users VALUES (%d, '%s', '%s')", 
        id, name, email);
    
    RistrettoResult result = ristretto_exec(db, sql);
    return (result == RISTRETTO_OK) ? 0 : -1;
}

// Batch insert example
int insert_test_data(RistrettoDB* db) {
    const char* users[][3] = {
        {"1", "Alice Johnson", "alice@example.com"},
        {"2", "Bob Smith", "bob@example.com"},
        {"3", "Carol Davis", "carol@example.com"},
        {"4", "David Wilson", "david@example.com"}
    };
    
    for (int i = 0; i < 4; i++) {
        char sql[512];
        snprintf(sql, sizeof(sql), 
            "INSERT INTO users VALUES (%s, '%s', '%s')",
            users[i][0], users[i][1], users[i][2]);
        
        if (ristretto_exec(db, sql) != RISTRETTO_OK) {
            fprintf(stderr, "Failed to insert user %d\n", i);
            return -1;
        }
    }
    
    printf("Inserted %d users\n", 4);
    return 0;
}
```

### Querying Data

```c
#include "db.h"

// Query callback function
void print_user(void* ctx, int n_cols, char** values, char** col_names) {
    printf("User: ");
    for (int i = 0; i < n_cols; i++) {
        printf("%s=%s ", col_names[i], values[i] ? values[i] : "NULL");
    }
    printf("\n");
}

// Count callback for aggregation
void count_callback(void* ctx, int n_cols, char** values, char** col_names) {
    int* count = (int*)ctx;
    if (values[0]) {
        *count = atoi(values[0]);
    }
}

// Query examples
int query_examples(RistrettoDB* db) {
    printf("All users:\n");
    ristretto_query(db, "SELECT * FROM users", print_user, NULL);
    
    printf("\nUsers with 'Smith' in name:\n");
    ristretto_query(db, "SELECT * FROM users WHERE name LIKE '%Smith%'", 
                   print_user, NULL);
    
    // Count query
    int user_count = 0;
    ristretto_query(db, "SELECT COUNT(*) FROM users", count_callback, &user_count);
    printf("\nTotal users: %d\n", user_count);
    
    return 0;
}
```

### Error Handling

```c
#include "db.h"

int robust_database_operation(const char* db_path) {
    RistrettoDB* db = ristretto_open(db_path);
    if (!db) {
        fprintf(stderr, "Failed to open database: %s\n", db_path);
        return -1;
    }
    
    // Try to create table
    RistrettoResult result = ristretto_exec(db, 
        "CREATE TABLE logs (timestamp INTEGER, level TEXT, message TEXT)");
    
    switch (result) {
        case RISTRETTO_OK:
            printf("Table created successfully\n");
            break;
            
        case RISTRETTO_PARSE_ERROR:
            fprintf(stderr, "SQL syntax error\n");
            ristretto_close(db);
            return -1;
            
        case RISTRETTO_IO_ERROR:
            fprintf(stderr, "File I/O error\n");
            ristretto_close(db);
            return -1;
            
        case RISTRETTO_NOMEM:
            fprintf(stderr, "Out of memory\n");
            ristretto_close(db);
            return -1;
            
        default:
            fprintf(stderr, "Unknown error: %s\n", 
                    ristretto_error_string(result));
            ristretto_close(db);
            return -1;
    }
    
    ristretto_close(db);
    return 0;
}
```

### Advanced Original API Examples

```c
// Application logging with Original API
typedef struct {
    RistrettoDB* db;
    int log_count;
} Logger;

Logger* logger_create(const char* db_path) {
    Logger* logger = malloc(sizeof(Logger));
    if (!logger) return NULL;
    
    logger->db = ristretto_open(db_path);
    if (!logger->db) {
        free(logger);
        return NULL;
    }
    
    // Create logs table
    ristretto_exec(logger->db, 
        "CREATE TABLE logs (id INTEGER, timestamp INTEGER, "
        "level TEXT, component TEXT, message TEXT)");
    
    logger->log_count = 0;
    return logger;
}

void logger_log(Logger* logger, const char* level, const char* component, 
                const char* message) {
    if (!logger) return;
    
    time_t now = time(NULL);
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "INSERT INTO logs VALUES (%d, %ld, '%s', '%s', '%s')",
        ++logger->log_count, now, level, component, message);
    
    ristretto_exec(logger->db, sql);
}

void logger_destroy(Logger* logger) {
    if (logger) {
        ristretto_close(logger->db);
        free(logger);
    }
}
```

---

## Table V2 Ultra-Fast API

### Basic Setup

```c
#include "table_v2.h"
#include <stdio.h>

int main() {
    // Create ultra-fast table with schema
    Table* table = table_create("events", 
        "CREATE TABLE events (timestamp INTEGER, user_id INTEGER, event TEXT(32))");
    
    if (!table) {
        fprintf(stderr, "Failed to create table\n");
        return 1;
    }
    
    printf("Table created with %zu columns\n", table->header->column_count);
    printf("Row size: %u bytes\n", table->header->row_size);
    
    table_close(table);
    return 0;
}
```

### Value Types and Creation

```c
#include "table_v2.h"

// Integer values
Value create_timestamp() {
    return value_integer(time(NULL));
}

Value create_user_id(int id) {
    return value_integer(id);
}

// Text values (remember to destroy!)
Value create_event_name(const char* event) {
    return value_text(event);  // Allocates memory
}

// Real values
Value create_score(double score) {
    return value_real(score);
}

// Null values
Value create_null_field() {
    return value_null();
}

// Example with all types
void demonstrate_value_types() {
    // Create table with all supported types
    Table* table = table_create("demo", 
        "CREATE TABLE demo (id INTEGER, name TEXT(50), score REAL)");
    
    // Create values
    Value values[3];
    values[0] = value_integer(42);
    values[1] = value_text("John Doe");
    values[2] = value_real(95.5);
    
    // Insert row
    if (table_append_row(table, values)) {
        printf("Row inserted successfully\n");
    }
    
    // IMPORTANT: Clean up text values
    value_destroy(&values[1]);
    
    table_close(table);
}
```

### High-Speed Insertion

```c
#include "table_v2.h"
#include <time.h>

// Ultra-fast logging example
int high_speed_logging_example() {
    Table* table = table_create("access_log", 
        "CREATE TABLE access_log (timestamp INTEGER, ip TEXT(16), "
        "status INTEGER, bytes INTEGER)");
    
    if (!table) return -1;
    
    printf("Starting high-speed insertion...\n");
    clock_t start = clock();
    
    // Insert 100,000 log entries as fast as possible
    for (int i = 0; i < 100000; i++) {
        Value values[4];
        values[0] = value_integer(time(NULL) + i);
        values[1] = value_text("192.168.1.100");
        values[2] = value_integer(200);
        values[3] = value_integer(1024 + (i % 10000));
        
        if (!table_append_row(table, values)) {
            fprintf(stderr, "Failed to insert row %d\n", i);
            value_destroy(&values[1]);
            break;
        }
        
        value_destroy(&values[1]);  // Clean up text
    }
    
    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    double rows_per_sec = 100000.0 / elapsed;
    
    printf("Inserted 100,000 rows in %.3f seconds\n", elapsed);
    printf("Throughput: %.0f rows/second\n", rows_per_sec);
    printf("Latency: %.0f ns/row\n", (elapsed * 1e9) / 100000);
    
    table_close(table);
    return 0;
}
```

### Memory Management Best Practices

```c
#include "table_v2.h"

// CORRECT: Proper memory management
int correct_memory_usage() {
    Table* table = table_create("users", 
        "CREATE TABLE users (id INTEGER, name TEXT(64), email TEXT(128))");
    
    Value values[3];
    values[0] = value_integer(1);
    values[1] = value_text("Alice Johnson");    // Allocates memory
    values[2] = value_text("alice@example.com"); // Allocates memory
    
    bool success = table_append_row(table, values);
    
    // CRITICAL: Always destroy text values
    value_destroy(&values[1]);
    value_destroy(&values[2]);
    
    table_close(table);
    return success ? 0 : -1;
}

// WRONG: Memory leak example
int memory_leak_example() {
    Table* table = table_create("users", "CREATE TABLE users (name TEXT(64))");
    
    Value values[1];
    values[0] = value_text("User Name");
    
    table_append_row(table, values);
    
    // BUG: Forgot to call value_destroy(&values[0])
    // This leaks the allocated string memory!
    
    table_close(table);
    return 0;  // Memory leaked!
}

// BEST PRACTICE: Helper function for safe text handling
typedef struct {
    Value* values;
    int count;
} ValueArray;

ValueArray* value_array_create(int count) {
    ValueArray* arr = malloc(sizeof(ValueArray));
    if (!arr) return NULL;
    
    arr->values = calloc(count, sizeof(Value));
    if (!arr->values) {
        free(arr);
        return NULL;
    }
    
    arr->count = count;
    return arr;
}

void value_array_destroy(ValueArray* arr) {
    if (!arr) return;
    
    for (int i = 0; i < arr->count; i++) {
        value_destroy(&arr->values[i]);
    }
    
    free(arr->values);
    free(arr);
}

// Safe usage with helper
int safe_insertion_example() {
    Table* table = table_create("products", 
        "CREATE TABLE products (id INTEGER, name TEXT(128), price REAL)");
    
    ValueArray* vals = value_array_create(3);
    vals->values[0] = value_integer(1001);
    vals->values[1] = value_text("Gaming Laptop");
    vals->values[2] = value_real(1299.99);
    
    bool success = table_append_row(table, vals->values);
    
    value_array_destroy(vals);  // Automatic cleanup
    table_close(table);
    
    return success ? 0 : -1;
}
```

### Querying and Scanning

```c
#include "table_v2.h"

// Query callback context
typedef struct {
    int count;
    int target_user_id;
} QueryContext;

// Callback function for table scanning
void user_events_callback(void* ctx, const Value* row) {
    QueryContext* query_ctx = (QueryContext*)ctx;
    
    // Assuming table schema: timestamp INTEGER, user_id INTEGER, event TEXT(32)
    if (row[1].type == COL_TYPE_INTEGER && 
        row[1].value.integer == query_ctx->target_user_id) {
        
        query_ctx->count++;
        
        printf("Event: timestamp=%ld, user_id=%ld, event=%s\n",
               row[0].value.integer,
               row[1].value.integer,
               row[2].value.text.data);
    }
}

// Query example
int query_user_events(Table* table, int user_id) {
    QueryContext ctx = { .count = 0, .target_user_id = user_id };
    
    printf("Finding events for user %d:\n", user_id);
    
    bool success = table_select(table, NULL, user_events_callback, &ctx);
    
    printf("Found %d events for user %d\n", ctx.count, user_id);
    
    return success ? ctx.count : -1;
}

// Aggregate query example
typedef struct {
    int total_events;
    int unique_users;
    int user_ids[1000];  // Simple array for demo
} AggregateContext;

void aggregate_callback(void* ctx, const Value* row) {
    AggregateContext* agg = (AggregateContext*)ctx;
    
    agg->total_events++;
    
    int user_id = (int)row[1].value.integer;
    
    // Simple unique user counting (not efficient, just for demo)
    bool found = false;
    for (int i = 0; i < agg->unique_users; i++) {
        if (agg->user_ids[i] == user_id) {
            found = true;
            break;
        }
    }
    
    if (!found && agg->unique_users < 1000) {
        agg->user_ids[agg->unique_users++] = user_id;
    }
}

int analyze_table_stats(Table* table) {
    AggregateContext agg = {0};
    
    table_select(table, NULL, aggregate_callback, &agg);
    
    printf("Table Statistics:\n");
    printf("  Total events: %d\n", agg.total_events);
    printf("  Unique users: %d\n", agg.unique_users);
    printf("  Table rows: %zu\n", table_get_row_count(table));
    
    return 0;
}
```

### Schema Design Patterns

```c
// Time-series data schema
Table* create_timeseries_table(const char* name) {
    char schema[512];
    snprintf(schema, sizeof(schema),
        "CREATE TABLE %s ("
        "timestamp INTEGER, "     // 8 bytes - epoch timestamp
        "sensor_id INTEGER, "     // 8 bytes - sensor identifier  
        "value REAL, "           // 8 bytes - measurement value
        "quality INTEGER"        // 8 bytes - quality indicator
        ")", name);              // Total: 32 bytes per row
    
    return table_create(name, schema);
}

// Log entry schema
Table* create_log_table(const char* name) {
    char schema[512];
    snprintf(schema, sizeof(schema),
        "CREATE TABLE %s ("
        "timestamp INTEGER, "     // 8 bytes
        "level INTEGER, "        // 8 bytes - 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
        "component TEXT(16), "   // 16 bytes - component name
        "message TEXT(128)"      // 128 bytes - log message
        ")", name);              // Total: 160 bytes per row
    
    return table_create(name, schema);
}

// Network packet metadata schema  
Table* create_packet_table(const char* name) {
    char schema[512];
    snprintf(schema, sizeof(schema),
        "CREATE TABLE %s ("
        "timestamp INTEGER, "     // 8 bytes
        "src_ip TEXT(16), "      // 16 bytes - "xxx.xxx.xxx.xxx"
        "dst_ip TEXT(16), "      // 16 bytes 
        "src_port INTEGER, "     // 8 bytes
        "dst_port INTEGER, "     // 8 bytes
        "protocol INTEGER, "     // 8 bytes - TCP=6, UDP=17, etc.
        "size INTEGER"           // 8 bytes - packet size
        ")", name);              // Total: 80 bytes per row
    
    return table_create(name, schema);
}

// Financial transaction schema
Table* create_transaction_table(const char* name) {
    char schema[512];
    snprintf(schema, sizeof(schema),
        "CREATE TABLE %s ("
        "timestamp INTEGER, "     // 8 bytes
        "account_id INTEGER, "   // 8 bytes  
        "amount REAL, "         // 8 bytes - transaction amount
        "type INTEGER, "        // 8 bytes - 0=debit, 1=credit
        "reference TEXT(32)"    // 32 bytes - transaction reference
        ")", name);             // Total: 64 bytes per row
    
    return table_create(name, schema);
}
```

---

## Real-World Examples

### Example 1: High-Speed IoT Telemetry System

```c
#include "table_v2.h"
#include <time.h>

typedef struct {
    Table* sensor_data;
    Table* device_status;
    int total_readings;
} IoTSystem;

IoTSystem* iot_system_create(void) {
    IoTSystem* system = malloc(sizeof(IoTSystem));
    if (!system) return NULL;
    
    // Sensor readings table (high-frequency data)
    system->sensor_data = table_create("sensor_data",
        "CREATE TABLE sensor_data ("
        "timestamp INTEGER, "
        "device_id INTEGER, "
        "temperature REAL, "
        "humidity REAL, "
        "battery_level INTEGER"
        ")");
    
    // Device status table (low-frequency data)  
    system->device_status = table_create("device_status",
        "CREATE TABLE device_status ("
        "timestamp INTEGER, "
        "device_id INTEGER, "
        "status INTEGER, "      // 0=offline, 1=online, 2=maintenance
        "firmware TEXT(16), "
        "location TEXT(64)"
        ")");
    
    if (!system->sensor_data || !system->device_status) {
        // Cleanup on failure
        if (system->sensor_data) table_close(system->sensor_data);
        if (system->device_status) table_close(system->device_status);
        free(system);
        return NULL;
    }
    
    system->total_readings = 0;
    return system;
}

// Record sensor reading (called frequently - optimized for speed)
bool iot_record_sensor_data(IoTSystem* system, int device_id, 
                           double temperature, double humidity, int battery) {
    Value values[5];
    values[0] = value_integer(time(NULL));
    values[1] = value_integer(device_id);
    values[2] = value_real(temperature);
    values[3] = value_real(humidity);
    values[4] = value_integer(battery);
    
    bool success = table_append_row(system->sensor_data, values);
    if (success) {
        system->total_readings++;
    }
    
    return success;
}

// Update device status (called rarely)
bool iot_update_device_status(IoTSystem* system, int device_id, int status,
                             const char* firmware, const char* location) {
    Value values[5];
    values[0] = value_integer(time(NULL));
    values[1] = value_integer(device_id);
    values[2] = value_integer(status);
    values[3] = value_text(firmware);
    values[4] = value_text(location);
    
    bool success = table_append_row(system->device_status, values);
    
    // Clean up text values
    value_destroy(&values[3]);
    value_destroy(&values[4]);
    
    return success;
}

// Simulate high-frequency data collection
int iot_simulate_data_collection(IoTSystem* system, int duration_seconds) {
    printf("Simulating IoT data collection for %d seconds...\n", duration_seconds);
    
    time_t start_time = time(NULL);
    int readings_count = 0;
    
    while (time(NULL) - start_time < duration_seconds) {
        // Simulate 3 devices reporting every 100ms
        for (int device = 1; device <= 3; device++) {
            double temp = 20.0 + (rand() % 200) / 10.0;  // 20-40°C
            double humidity = 30.0 + (rand() % 500) / 10.0;  // 30-80%
            int battery = 20 + (rand() % 80);  // 20-100%
            
            iot_record_sensor_data(system, device, temp, humidity, battery);
            readings_count++;
        }
        
        // Sleep 100ms (10 readings/second per device = 30 total/second)
        usleep(100000);
    }
    
    printf("Collected %d sensor readings\n", readings_count);
    printf("Average: %.1f readings/second\n", 
           (double)readings_count / duration_seconds);
    
    return readings_count;
}

void iot_system_destroy(IoTSystem* system) {
    if (system) {
        table_close(system->sensor_data);
        table_close(system->device_status);
        free(system);
    }
}
```

### Example 2: Security Audit Trail System

```c
#include "table_v2.h"

typedef enum {
    EVENT_LOGIN = 1,
    EVENT_LOGOUT = 2,
    EVENT_FILE_ACCESS = 3,
    EVENT_PRIVILEGE_ESCALATION = 4,
    EVENT_FAILED_LOGIN = 5,
    EVENT_SYSTEM_COMMAND = 6
} SecurityEventType;

typedef struct {
    Table* security_events;
    Table* file_access;
    int total_events;
} SecurityAuditSystem;

SecurityAuditSystem* security_audit_create(void) {
    SecurityAuditSystem* system = malloc(sizeof(SecurityAuditSystem));
    if (!system) return NULL;
    
    // Main security events table
    system->security_events = table_create("security_events",
        "CREATE TABLE security_events ("
        "timestamp INTEGER, "
        "event_type INTEGER, "
        "user_id INTEGER, "
        "source_ip TEXT(16), "
        "description TEXT(128)"
        ")");
    
    // File access audit table
    system->file_access = table_create("file_access",
        "CREATE TABLE file_access ("
        "timestamp INTEGER, "
        "user_id INTEGER, "
        "file_path TEXT(256), "
        "action INTEGER, "       // 1=read, 2=write, 3=delete, 4=execute
        "result INTEGER"         // 0=denied, 1=allowed
        ")");
    
    if (!system->security_events || !system->file_access) {
        if (system->security_events) table_close(system->security_events);
        if (system->file_access) table_close(system->file_access);
        free(system);
        return NULL;
    }
    
    system->total_events = 0;
    return system;
}

// Log security event
bool security_log_event(SecurityAuditSystem* system, SecurityEventType event_type,
                        int user_id, const char* source_ip, const char* description) {
    Value values[5];
    values[0] = value_integer(time(NULL));
    values[1] = value_integer(event_type);
    values[2] = value_integer(user_id);
    values[3] = value_text(source_ip);
    values[4] = value_text(description);
    
    bool success = table_append_row(system->security_events, values);
    
    value_destroy(&values[3]);
    value_destroy(&values[4]);
    
    if (success) {
        system->total_events++;
    }
    
    return success;
}

// Log file access
bool security_log_file_access(SecurityAuditSystem* system, int user_id,
                             const char* file_path, int action, int result) {
    Value values[5];
    values[0] = value_integer(time(NULL));
    values[1] = value_integer(user_id);
    values[2] = value_text(file_path);
    values[3] = value_integer(action);
    values[4] = value_integer(result);
    
    bool success = table_append_row(system->file_access, values);
    
    value_destroy(&values[2]);
    
    return success;
}

// Example usage simulating security events
int security_simulate_events(SecurityAuditSystem* system) {
    printf("Simulating security audit events...\n");
    
    // User login
    security_log_event(system, EVENT_LOGIN, 1001, "192.168.1.100", 
                      "User john_doe logged in");
    
    // File accesses
    security_log_file_access(system, 1001, "/etc/passwd", 1, 1);  // read allowed
    security_log_file_access(system, 1001, "/var/log/audit.log", 1, 0);  // read denied
    
    // Privilege escalation attempt
    security_log_event(system, EVENT_PRIVILEGE_ESCALATION, 1001, "192.168.1.100",
                      "User attempted sudo command");
    
    // System command
    security_log_event(system, EVENT_SYSTEM_COMMAND, 1001, "192.168.1.100",
                      "Executed: ls -la /home/admin");
    
    // Failed login attempt
    security_log_event(system, EVENT_FAILED_LOGIN, 0, "192.168.1.200",
                      "Failed login attempt for user 'admin'");
    
    printf("Logged %d security events\n", system->total_events);
    return system->total_events;
}

void security_audit_destroy(SecurityAuditSystem* system) {
    if (system) {
        table_close(system->security_events);
        table_close(system->file_access);
        free(system);
    }
}
```

### Example 3: Real-Time Analytics Ingestion

```c
#include "table_v2.h"

typedef struct {
    Table* page_views;
    Table* user_actions;
    Table* conversion_events;
    long total_events;
} AnalyticsSystem;

AnalyticsSystem* analytics_create(void) {
    AnalyticsSystem* system = malloc(sizeof(AnalyticsSystem));
    if (!system) return NULL;
    
    // Page view events (highest volume)
    system->page_views = table_create("page_views",
        "CREATE TABLE page_views ("
        "timestamp INTEGER, "
        "session_id INTEGER, "
        "page_id INTEGER, "
        "user_agent TEXT(64), "
        "referrer TEXT(128)"
        ")");
    
    // User action events (medium volume)
    system->user_actions = table_create("user_actions",
        "CREATE TABLE user_actions ("
        "timestamp INTEGER, "
        "session_id INTEGER, "
        "action_type INTEGER, "  // 1=click, 2=scroll, 3=hover, etc.
        "element_id TEXT(32), "
        "value TEXT(64)"
        ")");
    
    // Conversion events (low volume, high value)
    system->conversion_events = table_create("conversion_events",
        "CREATE TABLE conversion_events ("
        "timestamp INTEGER, "
        "session_id INTEGER, "
        "conversion_type INTEGER, "  // 1=signup, 2=purchase, 3=download
        "value REAL, "              // conversion value
        "metadata TEXT(128)"
        ")");
    
    if (!system->page_views || !system->user_actions || !system->conversion_events) {
        if (system->page_views) table_close(system->page_views);
        if (system->user_actions) table_close(system->user_actions);
        if (system->conversion_events) table_close(system->conversion_events);
        free(system);
        return NULL;
    }
    
    system->total_events = 0;
    return system;
}

// Record page view (high frequency)
bool analytics_record_page_view(AnalyticsSystem* system, long session_id, 
                               int page_id, const char* user_agent, const char* referrer) {
    Value values[5];
    values[0] = value_integer(time(NULL));
    values[1] = value_integer(session_id);
    values[2] = value_integer(page_id);
    values[3] = value_text(user_agent);
    values[4] = value_text(referrer);
    
    bool success = table_append_row(system->page_views, values);
    
    value_destroy(&values[3]);
    value_destroy(&values[4]);
    
    if (success) system->total_events++;
    return success;
}

// Record user action (medium frequency)
bool analytics_record_action(AnalyticsSystem* system, long session_id,
                            int action_type, const char* element_id, const char* value) {
    Value values[5];
    values[0] = value_integer(time(NULL));
    values[1] = value_integer(session_id);
    values[2] = value_integer(action_type);
    values[3] = value_text(element_id);
    values[4] = value_text(value);
    
    bool success = table_append_row(system->user_actions, values);
    
    value_destroy(&values[3]);
    value_destroy(&values[4]);
    
    if (success) system->total_events++;
    return success;
}

// Record conversion (low frequency, important)
bool analytics_record_conversion(AnalyticsSystem* system, long session_id,
                               int conversion_type, double value, const char* metadata) {
    Value values[5];
    values[0] = value_integer(time(NULL));
    values[1] = value_integer(session_id);
    values[2] = value_integer(conversion_type);
    values[3] = value_real(value);
    values[4] = value_text(metadata);
    
    bool success = table_append_row(system->conversion_events, values);
    
    value_destroy(&values[4]);
    
    if (success) system->total_events++;
    return success;
}

// Simulate high-volume analytics data
int analytics_simulate_traffic(AnalyticsSystem* system, int duration_seconds) {
    printf("Simulating analytics traffic for %d seconds...\n", duration_seconds);
    
    time_t start_time = time(NULL);
    int events_generated = 0;
    
    while (time(NULL) - start_time < duration_seconds) {
        long session_id = 1000000 + (rand() % 100000);
        
        // Generate page views (most common)
        if (rand() % 100 < 80) {  // 80% chance
            analytics_record_page_view(system, session_id, 
                                     rand() % 1000,  // page_id
                                     "Mozilla/5.0 Chrome/91.0", 
                                     "https://google.com");
            events_generated++;
        }
        
        // Generate user actions (medium frequency)
        if (rand() % 100 < 30) {  // 30% chance
            analytics_record_action(system, session_id,
                                  1,  // click
                                  "button_signup",
                                  "clicked");
            events_generated++;
        }
        
        // Generate conversions (rare but valuable)
        if (rand() % 100 < 2) {  // 2% chance
            analytics_record_conversion(system, session_id,
                                       2,  // purchase
                                       99.99,
                                       "product_id=123,discount=10%");
            events_generated++;
        }
        
        // Small delay to simulate realistic traffic
        usleep(10000);  // 10ms
    }
    
    printf("Generated %d analytics events\n", events_generated);
    printf("Total events in system: %ld\n", system->total_events);
    printf("Average: %.1f events/second\n", 
           (double)events_generated / duration_seconds);
    
    return events_generated;
}

void analytics_destroy(AnalyticsSystem* system) {
    if (system) {
        table_close(system->page_views);
        table_close(system->user_actions);
        table_close(system->conversion_events);
        free(system);
    }
}
```

---

## Performance Optimization

### Maximizing Write Performance

```c
// 1. Use fixed-width text fields for predictable performance
Table* create_optimized_log_table(void) {
    // Good: Fixed-width TEXT fields
    return table_create("optimized_logs",
        "CREATE TABLE optimized_logs ("
        "timestamp INTEGER, "      // 8 bytes
        "level INTEGER, "         // 8 bytes (instead of TEXT)
        "component TEXT(16), "    // 16 bytes fixed
        "message TEXT(128)"       // 128 bytes fixed
        ")");                     // Total: 160 bytes per row
}

// 2. Batch insertions for maximum throughput
int batch_insert_demo(Table* table, int batch_size) {
    clock_t start = clock();
    
    for (int i = 0; i < batch_size; i++) {
        Value values[4];
        values[0] = value_integer(time(NULL) + i);
        values[1] = value_integer(2);  // INFO level
        values[2] = value_text("auth");
        values[3] = value_text("User authentication successful");
        
        table_append_row(table, values);
        
        value_destroy(&values[2]);
        value_destroy(&values[3]);
    }
    
    // Force sync for accurate timing
    table_flush(table);
    
    double elapsed = ((double)(clock() - start)) / CLOCKS_PER_SEC;
    printf("Batch of %d: %.3f sec (%.0f rows/sec)\n", 
           batch_size, elapsed, batch_size / elapsed);
    
    return 0;
}

// 3. Minimize memory allocations
typedef struct {
    char component[16];
    char message[128];
} ReusableStrings;

int optimized_insertion_loop(Table* table, int count) {
    ReusableStrings strings;
    strcpy(strings.component, "optimized");
    strcpy(strings.message, "Reused string buffers for performance");
    
    clock_t start = clock();
    
    for (int i = 0; i < count; i++) {
        Value values[4];
        values[0] = value_integer(time(NULL) + i);
        values[1] = value_integer(1);
        values[2] = value_text(strings.component);  // Reuses same content
        values[3] = value_text(strings.message);
        
        table_append_row(table, values);
        
        // Still need to destroy (they allocate new memory each time)
        value_destroy(&values[2]);
        value_destroy(&values[3]);
    }
    
    double elapsed = ((double)(clock() - start)) / CLOCKS_PER_SEC;
    printf("Optimized %d insertions: %.0f rows/sec\n", 
           count, count / elapsed);
    
    return 0;
}
```

### Memory Usage Optimization

```c
// Monitor table memory usage
void print_table_stats(Table* table) {
    printf("Table Statistics:\n");
    printf("  Row count: %zu\n", table_get_row_count(table));
    printf("  Row size: %u bytes\n", table->header->row_size);
    printf("  Data size: %zu bytes\n", 
           table_get_row_count(table) * table->header->row_size);
    printf("  File size: %zu bytes\n", table->mapped_size);
    printf("  Utilization: %.1f%%\n", 
           (double)(table->write_offset) / table->mapped_size * 100);
}

// Control sync frequency for performance vs durability
void configure_sync_behavior(Table* table) {
    // Access internal sync controls (implementation detail)
    // In production, these might be API parameters
    
    printf("Current sync settings:\n");
    printf("  Rows since sync: %lu\n", table->rows_since_sync);
    printf("  Last sync: %lu ms ago\n", 
           get_time_ms() - table->last_sync_time_ms);
    
    // Manual sync when needed
    if (table->rows_since_sync > 1000) {
        printf("Forcing sync...\n");
        table_flush(table);
    }
}
```

### Query Performance Tips

```c
// Efficient table scanning patterns
typedef struct {
    time_t start_time;
    time_t end_time;
    int count;
} TimeRangeContext;

void time_range_callback(void* ctx, const Value* row) {
    TimeRangeContext* range = (TimeRangeContext*)ctx;
    
    time_t timestamp = row[0].value.integer;
    
    // Early exit optimization
    if (timestamp < range->start_time) return;
    if (timestamp > range->end_time) return;
    
    range->count++;
    
    // Process row data efficiently
    // Avoid string operations in hot path
}

int efficient_time_range_query(Table* table, time_t start, time_t end) {
    TimeRangeContext ctx = { .start_time = start, .end_time = end, .count = 0 };
    
    clock_t query_start = clock();
    table_select(table, NULL, time_range_callback, &ctx);
    double elapsed = ((double)(clock() - query_start)) / CLOCKS_PER_SEC;
    
    printf("Time range query: %d results in %.3f sec\n", ctx.count, elapsed);
    return ctx.count;
}
```

---

## Best Practices

### Schema Design Guidelines

```c
// 1. GOOD: Use appropriate data types for performance
Table* good_schema_design(void) {
    return table_create("events",
        "CREATE TABLE events ("
        "timestamp INTEGER, "      // Use INTEGER for timestamps
        "event_type INTEGER, "     // Use INTEGER for enums/codes
        "user_id INTEGER, "        // Use INTEGER for IDs
        "ip_address TEXT(16), "    // Use fixed TEXT for known max lengths
        "details TEXT(128)"        // Reasonable limit for variable content
        ")");
}

// 2. BAD: Inefficient schema design
Table* bad_schema_design(void) {
    return table_create("events",
        "CREATE TABLE events ("
        "timestamp TEXT(32), "     // BAD: TEXT for timestamps (parsing overhead)
        "event_type TEXT(64), "    // BAD: TEXT for simple enums
        "user_id TEXT(32), "       // BAD: TEXT for numeric IDs
        "ip_address TEXT(255), "   // BAD: Oversized for IP addresses
        "details TEXT(255)"        // BAD: Large TEXT fields waste space
        ")");
}

// 3. Calculate optimal row sizes
void analyze_row_size_efficiency(void) {
    printf("Row size analysis:\n");
    
    // Efficient schema: 8+8+8+16+128 = 168 bytes
    printf("  Efficient schema: 168 bytes/row\n");
    printf("  10M rows: %.1f MB\n", (168.0 * 10000000) / (1024*1024));
    
    // Inefficient schema: 32+64+32+255+255 = 638 bytes  
    printf("  Inefficient schema: 638 bytes/row\n");
    printf("  10M rows: %.1f MB\n", (638.0 * 10000000) / (1024*1024));
    
    printf("  Space savings: %.1fx\n", 638.0 / 168.0);
}
```

### Error Handling Patterns

```c
// Robust error handling example
typedef enum {
    APP_SUCCESS = 0,
    APP_ERROR_DB_CREATE = -1,
    APP_ERROR_DB_INSERT = -2,
    APP_ERROR_DB_QUERY = -3,
    APP_ERROR_INVALID_INPUT = -4
} AppResult;

AppResult robust_data_logger_create(const char* db_path, Table** out_table) {
    if (!db_path || !out_table) {
        return APP_ERROR_INVALID_INPUT;
    }
    
    *out_table = table_create("log_events",
        "CREATE TABLE log_events ("
        "timestamp INTEGER, "
        "level INTEGER, "
        "message TEXT(256)"
        ")");
    
    if (!*out_table) {
        fprintf(stderr, "Failed to create log table at: %s\n", db_path);
        return APP_ERROR_DB_CREATE;
    }
    
    printf("Log table created successfully\n");
    return APP_SUCCESS;
}

AppResult robust_log_message(Table* table, int level, const char* message) {
    if (!table || !message) {
        return APP_ERROR_INVALID_INPUT;
    }
    
    // Validate input
    if (level < 0 || level > 3) {
        fprintf(stderr, "Invalid log level: %d\n", level);
        return APP_ERROR_INVALID_INPUT;
    }
    
    if (strlen(message) > 255) {
        fprintf(stderr, "Log message too long: %zu characters\n", strlen(message));
        return APP_ERROR_INVALID_INPUT;
    }
    
    Value values[3];
    values[0] = value_integer(time(NULL));
    values[1] = value_integer(level);
    values[2] = value_text(message);
    
    bool success = table_append_row(table, values);
    
    value_destroy(&values[2]);
    
    if (!success) {
        fprintf(stderr, "Failed to insert log message\n");
        return APP_ERROR_DB_INSERT;
    }
    
    return APP_SUCCESS;
}

// Usage with proper error checking
int application_main(void) {
    Table* log_table = NULL;
    AppResult result;
    
    result = robust_data_logger_create("app.db", &log_table);
    if (result != APP_SUCCESS) {
        fprintf(stderr, "Failed to initialize logging: %d\n", result);
        return 1;
    }
    
    result = robust_log_message(log_table, 1, "Application started");
    if (result != APP_SUCCESS) {
        fprintf(stderr, "Failed to log startup message: %d\n", result);
        table_close(log_table);
        return 1;
    }
    
    // Simulate application work
    for (int i = 0; i < 100; i++) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Processing item %d", i);
        
        result = robust_log_message(log_table, 1, msg);
        if (result != APP_SUCCESS) {
            fprintf(stderr, "Failed to log processing message: %d\n", result);
            break;
        }
    }
    
    robust_log_message(log_table, 1, "Application finished");
    
    table_close(log_table);
    return 0;
}
```

### Resource Management

```c
// RAII-style resource management for C
typedef struct {
    Table** tables;
    int count;
    int capacity;
} TableManager;

TableManager* table_manager_create(int initial_capacity) {
    TableManager* manager = malloc(sizeof(TableManager));
    if (!manager) return NULL;
    
    manager->tables = calloc(initial_capacity, sizeof(Table*));
    if (!manager->tables) {
        free(manager);
        return NULL;
    }
    
    manager->count = 0;
    manager->capacity = initial_capacity;
    return manager;
}

bool table_manager_add(TableManager* manager, Table* table) {
    if (!manager || !table) return false;
    
    if (manager->count >= manager->capacity) {
        // Resize if needed
        int new_capacity = manager->capacity * 2;
        Table** new_tables = realloc(manager->tables, 
                                   new_capacity * sizeof(Table*));
        if (!new_tables) return false;
        
        manager->tables = new_tables;
        manager->capacity = new_capacity;
    }
    
    manager->tables[manager->count++] = table;
    return true;
}

void table_manager_destroy(TableManager* manager) {
    if (!manager) return;
    
    // Close all managed tables
    for (int i = 0; i < manager->count; i++) {
        if (manager->tables[i]) {
            table_close(manager->tables[i]);
        }
    }
    
    free(manager->tables);
    free(manager);
}

// Usage example with automatic cleanup
int managed_application(void) {
    TableManager* manager = table_manager_create(10);
    if (!manager) return -1;
    
    // Create multiple tables
    Table* logs = table_create("logs", 
        "CREATE TABLE logs (timestamp INTEGER, message TEXT(128))");
    Table* metrics = table_create("metrics",
        "CREATE TABLE metrics (timestamp INTEGER, value REAL)");
    
    // Add to manager for automatic cleanup
    table_manager_add(manager, logs);
    table_manager_add(manager, metrics);
    
    // Use tables...
    Value log_values[2];
    log_values[0] = value_integer(time(NULL));
    log_values[1] = value_text("Application started");
    table_append_row(logs, log_values);
    value_destroy(&log_values[1]);
    
    // Automatic cleanup on exit
    table_manager_destroy(manager);
    return 0;
}
```

---

## Troubleshooting

### Common Issues and Solutions

#### Issue 1: Memory Leaks with Text Values

```c
// PROBLEM: Memory leak
void memory_leak_example(Table* table) {
    Value values[2];
    values[0] = value_integer(123);
    values[1] = value_text("Some text");  // Allocates memory
    
    table_append_row(table, values);
    
    // BUG: Forgot to call value_destroy(&values[1])
    // This leaks the allocated string memory!
}

// SOLUTION: Always destroy text values
void correct_memory_usage(Table* table) {
    Value values[2];
    values[0] = value_integer(123);
    values[1] = value_text("Some text");
    
    table_append_row(table, values);
    
    // CORRECT: Clean up text values
    value_destroy(&values[1]);
}

// DEBUGGING: Check for memory leaks
void debug_memory_usage(void) {
    // Use valgrind on Linux:
    // valgrind --leak-check=full ./your_program
    
    // Use Instruments on macOS:
    // instruments -t Leaks ./your_program
    
    // Simple leak detection pattern
    printf("Starting memory test...\n");
    
    for (int i = 0; i < 1000; i++) {
        Value val = value_text("Test string");
        value_destroy(&val);  // Should free all memory
    }
    
    printf("Memory test complete\n");
}
```

#### Issue 2: Table Creation Failures

```c
// PROBLEM: Table creation fails silently
Table* debug_table_creation(const char* name, const char* schema) {
    printf("Creating table '%s' with schema: %s\n", name, schema);
    
    Table* table = table_create(name, schema);
    if (!table) {
        printf("Table creation failed. Possible causes:\n");
        printf("  1. Invalid schema syntax\n");
        printf("  2. Data directory creation failed\n");
        printf("  3. File permissions issue\n");
        printf("  4. Out of memory\n");
        printf("  5. Invalid table name\n");
        
        // Check if data directory exists
        struct stat st;
        if (stat("data", &st) != 0) {
            printf("  ERROR: 'data' directory does not exist\n");
        } else if (!S_ISDIR(st.st_mode)) {
            printf("  ERROR: 'data' exists but is not a directory\n");
        } else {
            printf("  'data' directory exists and is accessible\n");
        }
        
        return NULL;
    }
    
    printf("Table created successfully:\n");
    printf("  Columns: %u\n", table->header->column_count);
    printf("  Row size: %u bytes\n", table->header->row_size);
    printf("  File: data/%s.rdb\n", name);
    
    return table;
}
```

#### Issue 3: Performance Problems

```c
// PROBLEM: Slow insertions
void diagnose_slow_insertions(Table* table) {
    printf("Diagnosing insertion performance...\n");
    
    // Test batch sizes
    int batch_sizes[] = {100, 1000, 10000, 100000};
    int num_tests = sizeof(batch_sizes) / sizeof(batch_sizes[0]);
    
    for (int i = 0; i < num_tests; i++) {
        clock_t start = clock();
        
        for (int j = 0; j < batch_sizes[i]; j++) {
            Value values[2];
            values[0] = value_integer(j);
            values[1] = value_text("test");
            
            table_append_row(table, values);
            value_destroy(&values[1]);
        }
        
        double elapsed = ((double)(clock() - start)) / CLOCKS_PER_SEC;
        double rate = batch_sizes[i] / elapsed;
        
        printf("  Batch %d: %.0f rows/sec\n", batch_sizes[i], rate);
        
        if (rate < 100000) {
            printf("    WARNING: Low performance detected\n");
            printf("    Possible causes:\n");
            printf("      - Large text fields\n");
            printf("      - Frequent sync operations\n");
            printf("      - Memory fragmentation\n");
            printf("      - Debug build (use -O3)\n");
        }
    }
}

// SOLUTION: Performance optimization
void optimize_for_performance(void) {
    printf("Performance optimization checklist:\n");
    printf("  1. Use fixed-width text fields\n");
    printf("  2. Minimize text field sizes\n");
    printf("  3. Use INTEGER instead of TEXT for codes/enums\n");
    printf("  4. Batch insertions when possible\n");
    printf("  5. Compile with -O3 -march=native\n");
    printf("  6. Avoid frequent table_flush() calls\n");
    printf("  7. Pre-allocate file space if known\n");
}
```

#### Issue 4: File Access Problems

```c
// PROBLEM: File access errors
void debug_file_access(const char* table_name) {
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "data/%s.rdb", table_name);
    
    printf("Debugging file access for: %s\n", file_path);
    
    // Check file existence
    if (access(file_path, F_OK) != 0) {
        printf("  File does not exist\n");
    } else {
        printf("  File exists\n");
        
        // Check permissions
        if (access(file_path, R_OK) == 0) {
            printf("  File is readable\n");
        } else {
            printf("  ERROR: File is not readable\n");
        }
        
        if (access(file_path, W_OK) == 0) {
            printf("  File is writable\n");
        } else {
            printf("  ERROR: File is not writable\n");
        }
        
        // Check file size
        struct stat st;
        if (stat(file_path, &st) == 0) {
            printf("  File size: %ld bytes\n", st.st_size);
        }
    }
    
    // Check data directory
    if (access("data", F_OK) != 0) {
        printf("  ERROR: 'data' directory does not exist\n");
        printf("  Solution: Create directory or run from correct location\n");
    }
}
```

### Debugging Tips

```c
// Enable debug output
void enable_debug_mode(Table* table) {
    if (table) {
        printf("Table Debug Info:\n");
        printf("  Name: %s\n", table->name);
        printf("  File: %s\n", table->file_path);
        printf("  Mapped size: %zu bytes\n", table->mapped_size);
        printf("  Write offset: %zu bytes\n", table->write_offset);
        printf("  Rows: %lu\n", table->header->num_rows);
        printf("  Rows since sync: %lu\n", table->rows_since_sync);
        
        // Column information
        for (uint32_t i = 0; i < table->header->column_count; i++) {
            const ColumnDesc* col = &table->header->columns[i];
            printf("  Column %d: %s (%s, %d bytes @ offset %d)\n",
                   i, col->name, 
                   col->type == COL_TYPE_INTEGER ? "INTEGER" :
                   col->type == COL_TYPE_REAL ? "REAL" :
                   col->type == COL_TYPE_TEXT ? "TEXT" : "UNKNOWN",
                   col->length, col->offset);
        }
    }
}

// Performance profiling
void profile_operations(Table* table) {
    printf("Profiling table operations...\n");
    
    // Profile single insertion
    clock_t start = clock();
    Value values[2];
    values[0] = value_integer(1);
    values[1] = value_text("test");
    table_append_row(table, values);
    value_destroy(&values[1]);
    double single_insert = ((double)(clock() - start)) / CLOCKS_PER_SEC;
    
    printf("  Single insert: %.6f seconds\n", single_insert);
    
    // Profile table scan
    int row_count = 0;
    start = clock();
    table_select(table, NULL, (void(*)(void*,const Value*))NULL, &row_count);
    double scan_time = ((double)(clock() - start)) / CLOCKS_PER_SEC;
    
    printf("  Table scan: %.6f seconds\n", scan_time);
    printf("  Scan rate: %.0f rows/sec\n", 
           table_get_row_count(table) / scan_time);
}
```

---

## Production Deployment

### Deployment Architecture Guidelines

#### Single Application Embedding

```c
// Recommended pattern for single application deployment
typedef struct {
    RistrettoDB* config_db;    // Configuration and metadata
    Table* metrics_table;      // High-frequency metrics
    Table* events_table;       // Application events
    Table* audit_table;        // Security audit trail
} ProductionSystem;

ProductionSystem* system_create(const char* data_dir, const char* app_name) {
    ProductionSystem* sys = malloc(sizeof(ProductionSystem));
    if (!sys) return NULL;
    
    // Create data directory
    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", data_dir);
    system(mkdir_cmd);
    
    // Change to data directory for all database files
    if (chdir(data_dir) != 0) {
        free(sys);
        return NULL;
    }
    
    // Initialize configuration database
    char config_path[256];
    snprintf(config_path, sizeof(config_path), "%s_config.db", app_name);
    sys->config_db = ristretto_open(config_path);
    
    if (!sys->config_db) {
        free(sys);
        return NULL;
    }
    
    // Create configuration tables
    ristretto_exec(sys->config_db, 
        "CREATE TABLE app_config (key TEXT, value TEXT, updated INTEGER)");
    ristretto_exec(sys->config_db, 
        "CREATE TABLE schema_versions (component TEXT, version INTEGER)");
    
    // Initialize high-performance tables
    sys->metrics_table = table_create("metrics",
        "CREATE TABLE metrics ("
        "timestamp INTEGER, metric_name TEXT(32), value REAL, tags TEXT(64))");
    
    sys->events_table = table_create("events",
        "CREATE TABLE events ("
        "timestamp INTEGER, level INTEGER, component TEXT(32), "
        "event_type TEXT(32), data TEXT(256))");
    
    sys->audit_table = table_create("audit",
        "CREATE TABLE audit ("
        "timestamp INTEGER, user_id INTEGER, action TEXT(64), "
        "resource TEXT(128), result INTEGER)");
    
    if (!sys->metrics_table || !sys->events_table || !sys->audit_table) {
        // Cleanup on failure
        if (sys->config_db) ristretto_close(sys->config_db);
        if (sys->metrics_table) table_close(sys->metrics_table);
        if (sys->events_table) table_close(sys->events_table);
        if (sys->audit_table) table_close(sys->audit_table);
        free(sys);
        return NULL;
    }
    
    return sys;
}
```

#### Container Deployment

**Dockerfile for RistrettoDB Applications:**

```dockerfile
# Multi-stage build for minimal production image
FROM alpine:latest AS builder

# Install build dependencies
RUN apk add --no-cache gcc musl-dev make git

# Build RistrettoDB
WORKDIR /build
COPY . .
RUN make lib

# Production stage
FROM alpine:latest

# Install minimal runtime dependencies
RUN apk add --no-cache libc6-compat

# Create application user
RUN adduser -D -s /bin/sh ristretto

# Copy application and library
COPY --from=builder /build/lib/libristretto.a /usr/local/lib/
COPY --from=builder /build/embed/ristretto.h /usr/local/include/
COPY --from=builder /build/myapp /usr/local/bin/

# Create data directory
RUN mkdir -p /data && chown ristretto:ristretto /data

# Switch to application user
USER ristretto
WORKDIR /data

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD /usr/local/bin/myapp --health-check || exit 1

EXPOSE 8080

CMD ["/usr/local/bin/myapp", "--data-dir", "/data"]
```

**Docker Compose for Production:**

```yaml
version: '3.8'

services:
  ristretto-app:
    build: .
    restart: unless-stopped
    volumes:
      - ristretto_data:/data
      - ./config:/config:ro
    environment:
      - RISTRETTO_LOG_LEVEL=INFO
      - RISTRETTO_METRICS_INTERVAL=60
    ports:
      - "8080:8080"
    healthcheck:
      test: ["CMD", "/usr/local/bin/myapp", "--health-check"]
      interval: 30s
      timeout: 10s
      retries: 3
    
  ristretto-monitor:
    image: grafana/grafana:latest
    restart: unless-stopped
    volumes:
      - grafana_data:/var/lib/grafana
      - ./monitoring:/etc/grafana/provisioning:ro
    ports:
      - "3000:3000"
    depends_on:
      - ristretto-app

volumes:
  ristretto_data:
  grafana_data:
```

### Performance Tuning for Production

#### Memory Management

```c
// Production memory management patterns
typedef struct {
    char** string_pool;      // Pre-allocated string pool
    int pool_size;
    int pool_index;
    char* text_buffer;       // Reusable text buffer
    size_t buffer_size;
} MemoryPool;

MemoryPool* memory_pool_create(int pool_size, size_t buffer_size) {
    MemoryPool* pool = malloc(sizeof(MemoryPool));
    if (!pool) return NULL;
    
    pool->string_pool = calloc(pool_size, sizeof(char*));
    pool->text_buffer = malloc(buffer_size);
    
    if (!pool->string_pool || !pool->text_buffer) {
        free(pool->string_pool);
        free(pool->text_buffer);
        free(pool);
        return NULL;
    }
    
    pool->pool_size = pool_size;
    pool->pool_index = 0;
    pool->buffer_size = buffer_size;
    
    return pool;
}

// Optimized logging with memory pool
void optimized_log_event(Table* table, MemoryPool* pool, 
                        int level, const char* component, const char* message) {
    Value values[4];
    values[0] = value_integer(time(NULL));
    values[1] = value_integer(level);
    
    // Use string pool to avoid allocations
    strncpy(pool->text_buffer, component, pool->buffer_size - 1);
    values[2] = value_text(pool->text_buffer);
    
    strncpy(pool->text_buffer + 256, message, pool->buffer_size - 257);
    values[3] = value_text(pool->text_buffer + 256);
    
    table_append_row(table, values);
    
    // Clean up
    value_destroy(&values[2]);
    value_destroy(&values[3]);
}
```

#### File System Optimization

```c
// Production file system configuration
int configure_production_storage(const char* data_dir) {
    // Ensure data directory exists with proper permissions
    struct stat st;
    if (stat(data_dir, &st) != 0) {
        if (mkdir(data_dir, 0755) != 0) {
            return -1;
        }
    }
    
    // Set optimal file system parameters
    // These would be system-specific optimizations
    
    // Check available disk space
    struct statvfs vfs;
    if (statvfs(data_dir, &vfs) == 0) {
        unsigned long available_gb = (vfs.f_bavail * vfs.f_frsize) / (1024*1024*1024);
        if (available_gb < 1) {  // Less than 1GB available
            fprintf(stderr, "Warning: Low disk space (%lu GB available)\n", available_gb);
        }
    }
    
    return 0;
}

// Backup and rotation strategy
int rotate_table_files(const char* table_name, int max_files) {
    // Implementation would handle file rotation
    // e.g., rename current file to .1, .2, etc.
    // and create new file for continued writes
    
    char current_file[512];
    snprintf(current_file, sizeof(current_file), "data/%s.rdb", table_name);
    
    // Check file size and rotate if needed
    struct stat st;
    if (stat(current_file, &st) == 0) {
        if (st.st_size > 1024*1024*1024) {  // 1GB limit
            // Perform rotation
            char backup_file[512];
            snprintf(backup_file, sizeof(backup_file), "data/%s.rdb.%ld", 
                    table_name, time(NULL));
            return rename(current_file, backup_file);
        }
    }
    
    return 0;
}
```

### Monitoring and Observability

#### Application Metrics

```c
// Built-in metrics collection
typedef struct {
    Table* metrics_table;
    long requests_processed;
    long errors_encountered;
    double avg_response_time;
    time_t start_time;
} ApplicationMetrics;

ApplicationMetrics* metrics_create(void) {
    ApplicationMetrics* metrics = malloc(sizeof(ApplicationMetrics));
    if (!metrics) return NULL;
    
    metrics->metrics_table = table_create("app_metrics",
        "CREATE TABLE app_metrics ("
        "timestamp INTEGER, metric_name TEXT(32), "
        "value REAL, labels TEXT(128))");
    
    if (!metrics->metrics_table) {
        free(metrics);
        return NULL;
    }
    
    metrics->requests_processed = 0;
    metrics->errors_encountered = 0;
    metrics->avg_response_time = 0.0;
    metrics->start_time = time(NULL);
    
    return metrics;
}

void metrics_record(ApplicationMetrics* metrics, const char* metric_name, 
                   double value, const char* labels) {
    Value values[4];
    values[0] = value_integer(time(NULL));
    values[1] = value_text(metric_name);
    values[2] = value_real(value);
    values[3] = value_text(labels ? labels : "");
    
    table_append_row(metrics->metrics_table, values);
    
    value_destroy(&values[1]);
    value_destroy(&values[3]);
}

// Periodic metrics export
void metrics_export_prometheus(ApplicationMetrics* metrics, FILE* output) {
    fprintf(output, "# HELP ristretto_requests_total Total requests processed\n");
    fprintf(output, "# TYPE ristretto_requests_total counter\n");
    fprintf(output, "ristretto_requests_total %ld\n", metrics->requests_processed);
    
    fprintf(output, "# HELP ristretto_errors_total Total errors encountered\n");
    fprintf(output, "# TYPE ristretto_errors_total counter\n");
    fprintf(output, "ristretto_errors_total %ld\n", metrics->errors_encountered);
    
    fprintf(output, "# HELP ristretto_response_time_avg Average response time\n");
    fprintf(output, "# TYPE ristretto_response_time_avg gauge\n");
    fprintf(output, "ristretto_response_time_avg %.3f\n", metrics->avg_response_time);
    
    time_t uptime = time(NULL) - metrics->start_time;
    fprintf(output, "# HELP ristretto_uptime_seconds Application uptime\n");
    fprintf(output, "# TYPE ristretto_uptime_seconds counter\n");
    fprintf(output, "ristretto_uptime_seconds %ld\n", uptime);
}
```

#### Health Checks

```c
// Application health check system
typedef struct {
    bool database_healthy;
    bool storage_healthy;
    bool memory_healthy;
    time_t last_check;
    char error_message[256];
} HealthStatus;

HealthStatus* health_check_create(void) {
    HealthStatus* health = malloc(sizeof(HealthStatus));
    if (!health) return NULL;
    
    health->database_healthy = true;
    health->storage_healthy = true;
    health->memory_healthy = true;
    health->last_check = time(NULL);
    health->error_message[0] = '\0';
    
    return health;
}

bool health_check_run(HealthStatus* health, ProductionSystem* system) {
    health->last_check = time(NULL);
    health->error_message[0] = '\0';
    
    // Check database connectivity
    if (system->config_db) {
        RistrettoResult result = ristretto_exec(system->config_db, 
            "SELECT 1");  // Simple connectivity test
        health->database_healthy = (result == RISTRETTO_OK);
        if (!health->database_healthy) {
            strncpy(health->error_message, "Database connectivity failed", 
                   sizeof(health->error_message) - 1);
        }
    }
    
    // Check storage space
    struct statvfs vfs;
    if (statvfs(".", &vfs) == 0) {
        unsigned long available_mb = (vfs.f_bavail * vfs.f_frsize) / (1024*1024);
        health->storage_healthy = (available_mb > 100);  // 100MB minimum
        if (!health->storage_healthy) {
            snprintf(health->error_message, sizeof(health->error_message),
                    "Low disk space: %lu MB available", available_mb);
        }
    }
    
    // Check memory usage (simplified)
    // In production, you'd use more sophisticated memory monitoring
    health->memory_healthy = true;  // Placeholder
    
    return health->database_healthy && health->storage_healthy && health->memory_healthy;
}

// REST endpoint for health checks
void health_check_endpoint(HealthStatus* health, FILE* response) {
    bool healthy = health_check_run(health, NULL);  // Assumes global system
    
    fprintf(response, "HTTP/1.1 %s\r\n", healthy ? "200 OK" : "503 Service Unavailable");
    fprintf(response, "Content-Type: application/json\r\n");
    fprintf(response, "Cache-Control: no-cache\r\n\r\n");
    
    fprintf(response, "{\n");
    fprintf(response, "  \"status\": \"%s\",\n", healthy ? "healthy" : "unhealthy");
    fprintf(response, "  \"timestamp\": %ld,\n", health->last_check);
    fprintf(response, "  \"checks\": {\n");
    fprintf(response, "    \"database\": %s,\n", 
            health->database_healthy ? "true" : "false");
    fprintf(response, "    \"storage\": %s,\n", 
            health->storage_healthy ? "true" : "false");
    fprintf(response, "    \"memory\": %s\n", 
            health->memory_healthy ? "true" : "false");
    fprintf(response, "  }");
    
    if (health->error_message[0] != '\0') {
        fprintf(response, ",\n  \"error\": \"%s\"", health->error_message);
    }
    
    fprintf(response, "\n}\n");
}
```

### Security Considerations

#### Access Control

```c
// Simple access control for embedded applications
typedef struct {
    char username[64];
    char password_hash[64];
    int permissions;  // Bitmask of permissions
    time_t last_login;
} User;

typedef struct {
    User* users;
    int user_count;
    int max_users;
    Table* audit_log;
} AccessControl;

#define PERM_READ    (1 << 0)
#define PERM_WRITE   (1 << 1)
#define PERM_ADMIN   (1 << 2)

AccessControl* access_control_create(int max_users) {
    AccessControl* ac = malloc(sizeof(AccessControl));
    if (!ac) return NULL;
    
    ac->users = calloc(max_users, sizeof(User));
    if (!ac->users) {
        free(ac);
        return NULL;
    }
    
    ac->audit_log = table_create("security_audit",
        "CREATE TABLE security_audit ("
        "timestamp INTEGER, username TEXT(64), action TEXT(128), "
        "success INTEGER, ip_address TEXT(16))");
    
    if (!ac->audit_log) {
        free(ac->users);
        free(ac);
        return NULL;
    }
    
    ac->user_count = 0;
    ac->max_users = max_users;
    
    return ac;
}

bool access_control_authenticate(AccessControl* ac, const char* username, 
                               const char* password, const char* ip_address) {
    // Find user
    User* user = NULL;
    for (int i = 0; i < ac->user_count; i++) {
        if (strcmp(ac->users[i].username, username) == 0) {
            user = &ac->users[i];
            break;
        }
    }
    
    bool success = false;
    if (user) {
        // In production, use proper password hashing (bcrypt, etc.)
        // This is simplified for example
        char password_hash[64];
        snprintf(password_hash, sizeof(password_hash), "hash_%s", password);
        success = (strcmp(user->password_hash, password_hash) == 0);
        
        if (success) {
            user->last_login = time(NULL);
        }
    }
    
    // Log authentication attempt
    Value audit_values[5];
    audit_values[0] = value_integer(time(NULL));
    audit_values[1] = value_text(username);
    audit_values[2] = value_text("LOGIN_ATTEMPT");
    audit_values[3] = value_integer(success ? 1 : 0);
    audit_values[4] = value_text(ip_address);
    
    table_append_row(ac->audit_log, audit_values);
    
    value_destroy(&audit_values[1]);
    value_destroy(&audit_values[2]);
    value_destroy(&audit_values[4]);
    
    return success;
}
```

### Configuration Management

```c
// Production configuration system
typedef struct {
    RistrettoDB* config_db;
    char config_file[512];
    time_t last_reload;
} ConfigManager;

ConfigManager* config_manager_create(const char* config_file) {
    ConfigManager* cm = malloc(sizeof(ConfigManager));
    if (!cm) return NULL;
    
    strncpy(cm->config_file, config_file, sizeof(cm->config_file) - 1);
    cm->config_file[sizeof(cm->config_file) - 1] = '\0';
    
    cm->config_db = ristretto_open("application_config.db");
    if (!cm->config_db) {
        free(cm);
        return NULL;
    }
    
    // Create configuration tables
    ristretto_exec(cm->config_db,
        "CREATE TABLE config_values (key TEXT, value TEXT, type TEXT, "
        "description TEXT, updated_at INTEGER)");
    
    ristretto_exec(cm->config_db,
        "CREATE TABLE config_history (key TEXT, old_value TEXT, new_value TEXT, "
        "changed_by TEXT, changed_at INTEGER)");
    
    cm->last_reload = time(NULL);
    
    return cm;
}

const char* config_get_string(ConfigManager* cm, const char* key, 
                             const char* default_value) {
    // Implementation would query the database for the configuration value
    // This is a simplified placeholder
    static char value[256];
    
    char sql[512];
    snprintf(sql, sizeof(sql), 
            "SELECT value FROM config_values WHERE key = '%s'", key);
    
    // In a full implementation, you'd execute this query and return the result
    // For now, return the default value
    return default_value;
}

int config_set_string(ConfigManager* cm, const char* key, const char* value,
                     const char* changed_by) {
    char sql[1024];
    snprintf(sql, sizeof(sql),
            "INSERT OR REPLACE INTO config_values "
            "(key, value, type, updated_at) VALUES ('%s', '%s', 'string', %ld)",
            key, value, time(NULL));
    
    RistrettoResult result = ristretto_exec(cm->config_db, sql);
    
    if (result == RISTRETTO_OK) {
        // Log the change
        snprintf(sql, sizeof(sql),
                "INSERT INTO config_history "
                "(key, new_value, changed_by, changed_at) "
                "VALUES ('%s', '%s', '%s', %ld)",
                key, value, changed_by, time(NULL));
        ristretto_exec(cm->config_db, sql);
    }
    
    return (result == RISTRETTO_OK) ? 0 : -1;
}
```

---

## Testing and Validation

### Comprehensive Test Suite

RistrettoDB includes a comprehensive test suite that validates all examples in this programming manual:

```bash
# Run the comprehensive test suite
make test-comprehensive
```

**What it tests:**
- All Table V2 API examples from this manual
- Schema parsing and value type handling
- High-speed insertion performance claims
- Memory management best practices
- Real-world scenarios (IoT, security, analytics)
- Performance targets (>1M rows/sec, <1000ns latency)
- File growth and error handling

**Expected Results:**
```
RistrettoDB Comprehensive Test Suite
====================================
Validating all programming manual claims...

All programming manual claims validated
Real-world scenarios working
Error handling robust
Performance claims verified

Total tests: 14
Passed: 14
Failed: 0
```

### Other Test Suites

```bash
# Individual test suites
make test-v2        # Table V2 basic functionality
make test-stress    # High-volume stress testing
make test-original  # Original SQL API validation
make test-all       # Run all test suites
```

### Performance Validation

The test suite validates these performance claims:
- **Throughput**: >1M rows/sec (typically achieves 7-11M rows/sec)
- **Latency**: <1000ns per row (typically achieves 88-215ns)
- **Memory**: Efficient handling of large text fields
- **Scalability**: File growth up to multiple GB

> **Tip**: Run `make test-comprehensive` after building to ensure all examples work correctly on your specific system configuration.

---

## Conclusion

This programming manual provides comprehensive examples for both RistrettoDB APIs:

- **Original SQL API**: Best for general embedded SQL needs with 2.8x SQLite performance
- **Table V2 Ultra-Fast API**: Best for high-speed writes with 4.6M rows/sec performance

### Key Takeaways

1. **Choose the right API** based on your performance vs compatibility needs
2. **Manage memory carefully** - always destroy text values
3. **Design schemas efficiently** - use appropriate data types and field sizes
4. **Handle errors robustly** - check return values and validate inputs
5. **Optimize for your use case** - batch operations, minimize allocations
6. **Profile and debug** - measure performance and investigate issues

### Getting Help

- Check the main README.md for additional information
- Run the test suites to verify your installation
- Use the benchmark tools to measure performance on your hardware
- Enable debug output when troubleshooting issues

For the best performance, compile with `-O3 -march=native` and design your schemas carefully. RistrettoDB excels at high-speed append-only workloads - leverage its strengths for maximum benefit.
