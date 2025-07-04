# RistrettoDB Python Bindings

High-performance Python bindings for RistrettoDB, the tiny, blazingly fast, embeddable SQL engine.

## Features

- **Original SQL API**: 2.8x faster than SQLite for general SQL operations
- **Table V2 Ultra-Fast API**: 4.57x faster than SQLite for append-only workloads
- **Zero Dependencies**: Pure Python using ctypes (no additional packages required)
- **Context Managers**: Automatic resource cleanup with `with` statements
- **Type Safety**: Full type hints for better IDE support
- **Error Handling**: Comprehensive exception handling with detailed error messages

## Performance

| API | Use Case | Performance vs SQLite |
|-----|----------|----------------------|
| **Original SQL** | General SQL operations | **2.8x faster** |
| **Table V2** | High-speed logging, IoT data | **4.57x faster** |

## Quick Start

### 1. Build RistrettoDB Library

```bash
# From the RistrettoDB root directory
cd ../../
make lib
```

### 2. Run the Example

```bash
python3 example.py
```

### 3. Use in Your Project

```python
from ristretto import RistrettoDB, RistrettoTable, RistrettoValue

# Original SQL API - General Purpose
with RistrettoDB("myapp.db") as db:
    db.exec("CREATE TABLE users (id INTEGER, name TEXT, score REAL)")
    db.exec("INSERT INTO users VALUES (1, 'Alice', 95.5)")
    
    results = db.query("SELECT * FROM users WHERE score > 90")
    for row in results:
        print(f"User: {row['name']}, Score: {row['score']}")

# Table V2 API - Ultra-Fast Writes
with RistrettoTable.create("events", 
                         "CREATE TABLE events (timestamp INTEGER, event TEXT(32))") as table:
    
    values = [
        RistrettoValue.integer(1672531200),
        RistrettoValue.text("user_login")
    ]
    table.append_row(values)
    
    print(f"Total events: {table.get_row_count()}")
```

## API Reference

### RistrettoDB (Original SQL API)

#### Constructor
```python
db = RistrettoDB(filename: str)
```

#### Methods
- `exec(sql: str)` - Execute DDL/DML statements
- `query(sql: str, callback=None) -> List[dict]` - Execute SELECT queries
- `close()` - Close database connection
- `version() -> str` - Get library version (static method)

#### Context Manager
```python
with RistrettoDB("mydb.db") as db:
    db.exec("CREATE TABLE test (id INTEGER)")
    # Automatically closed when exiting with block
```

### RistrettoTable (Table V2 Ultra-Fast API)

#### Creation/Opening
```python
table = RistrettoTable.create(name: str, schema_sql: str)
table = RistrettoTable.open(name: str)
```

#### Methods
- `append_row(values: List[RistrettoValue]) -> bool` - High-speed row insertion
- `get_row_count() -> int` - Get total number of rows
- `close()` - Close table

#### Context Manager
```python
with RistrettoTable.create("logs", "CREATE TABLE logs (timestamp INTEGER)") as table:
    table.append_row([RistrettoValue.integer(1672531200)])
    # Automatically closed when exiting with block
```

### RistrettoValue (Value Types)

#### Factory Methods
```python
RistrettoValue.integer(value: int)      # Create integer value
RistrettoValue.real(value: float)       # Create real/float value  
RistrettoValue.text(value: str)         # Create text value
RistrettoValue.null()                   # Create null value
```

#### Properties
- `type: RistrettoColumnType` - Column type (INTEGER, REAL, TEXT, NULLABLE)
- `value: Any` - The actual value
- `is_null: bool` - Whether value is null

## Error Handling

```python
from ristretto import RistrettoError, RistrettoResult

try:
    with RistrettoDB("mydb.db") as db:
        db.exec("INVALID SQL")
except RistrettoError as e:
    print(f"Database error: {e}")
    print(f"Error code: {e.result_code}")
```

## Use Cases

### High-Speed Data Logging
```python
# IoT sensor data collection
with RistrettoTable.create("sensors", 
    "CREATE TABLE sensors (timestamp INTEGER, temp REAL, device_id INTEGER)") as table:
    
    for reading in sensor_stream():
        values = [
            RistrettoValue.integer(reading.timestamp),
            RistrettoValue.real(reading.temperature),
            RistrettoValue.integer(reading.device_id)
        ]
        table.append_row(values)
```

### Analytics Event Tracking
```python
# Web analytics event ingestion
with RistrettoTable.create("events", 
    "CREATE TABLE events (timestamp INTEGER, user_id INTEGER, action TEXT(32))") as events:
    
    for event in event_stream():
        events.append_row([
            RistrettoValue.integer(event.timestamp),
            RistrettoValue.integer(event.user_id),
            RistrettoValue.text(event.action)
        ])
```

### Security Audit Trails
```python
# Tamper-evident security logging
with RistrettoDB("audit.db") as db:
    db.exec("CREATE TABLE audit_log (timestamp INTEGER, user TEXT, action TEXT)")
    
    for audit_event in security_events():
        db.exec(f"INSERT INTO audit_log VALUES ({audit_event.timestamp}, "
               f"'{audit_event.user}', '{audit_event.action}')")
```

## Performance Tips

1. **Use Table V2 for Write-Heavy Workloads**: 4.57x faster than SQLite
2. **Batch Operations**: Group multiple inserts when possible
3. **Context Managers**: Always use `with` statements for automatic cleanup
4. **Text Value Cleanup**: Text values are automatically managed
5. **Memory Mapping**: RistrettoDB uses memory-mapped I/O for zero-copy performance

## Requirements

- Python 3.6+
- RistrettoDB library built (`make lib`)
- POSIX-compliant system (Linux, macOS, BSD)

## Installation

1. **Build RistrettoDB**:
   ```bash
   cd ../../ && make lib
   ```

2. **Copy Python bindings**:
   ```bash
   cp examples/python/ristretto.py your_project/
   ```

3. **Import and use**:
   ```python
   from ristretto import RistrettoDB
   ```

## Architecture

The Python bindings use `ctypes` to interface with the RistrettoDB C library:

```
Python Application
       ↓
  Python Bindings (ristretto.py)
       ↓ ctypes
  RistrettoDB C Library (libristretto.so)
       ↓
  Memory-Mapped Files
```

## Thread Safety

RistrettoDB is currently single-threaded. For multi-threaded applications:

1. Use separate database instances per thread
2. Implement application-level locking if sharing databases
3. Consider using separate files for different threads

## Troubleshooting

### Library Not Found
```
RuntimeError: Could not find libristretto.so
```
**Solution**: Build the library first:
```bash
cd ../../ && make lib
```

### Permission Denied
```
OSError: [Errno 13] Permission denied
```
**Solution**: Check file permissions or run with appropriate privileges

### Invalid SQL Syntax
```
RistrettoError: PARSE_ERROR
```
**Solution**: Check SQL syntax - RistrettoDB supports a subset of SQL

## Contributing

Found a bug or want to improve the Python bindings? Please open an issue or submit a pull request at the main RistrettoDB repository.

## License

MIT License - same as RistrettoDB core library.