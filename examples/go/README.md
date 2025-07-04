# RistrettoDB Go Bindings

High-performance Go bindings for RistrettoDB, the tiny, blazingly fast, embeddable SQL engine.

## Features

- **Original SQL API**: 2.8x faster than SQLite for general SQL operations
- **Table V2 Ultra-Fast API**: 4.57x faster than SQLite for append-only workloads
- **Native Go Integration**: Uses cgo for seamless C library integration
- **Memory Safety**: Automatic resource cleanup with finalizers
- **Concurrent Access**: Thread-safe operations with mutex protection
- **Zero Dependencies**: Pure Go with only standard library dependencies

## Performance

| API | Use Case | Performance vs SQLite |
|-----|----------|----------------------|
| **Original SQL** | Web APIs, microservices | **2.8x faster** |
| **Table V2** | Analytics, IoT, trading systems | **4.57x faster** |

## Quick Start

### 1. Build RistrettoDB Library

```bash
# From the RistrettoDB root directory
cd ../../
make lib
```

### 2. Set Up Environment

```bash
# Set library path for cgo
export CGO_LDFLAGS="-L../../lib"
export LD_LIBRARY_PATH="../../lib:$LD_LIBRARY_PATH"
```

### 3. Run the Example

```bash
go run example.go
```

### 4. Use in Your Project

```go
package main

import (
    "fmt"
    "log"
    "./ristretto" // Adjust path as needed
)

func main() {
    // Original SQL API - General Purpose
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

    // Table V2 API - Ultra-Fast Writes
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

## Installation

### Copy Bindings

```bash
# Copy to your project
cp examples/go/ristretto.go your_project/
```

### Go Module Setup

```bash
cd your_project
go mod init your-project
```

### Build Configuration

```bash
# Set library paths
export CGO_LDFLAGS="-L/path/to/ristrettodb/lib"
export LD_LIBRARY_PATH="/path/to/ristrettodb/lib:$LD_LIBRARY_PATH"

# Build your application
go build -ldflags "-L/path/to/ristrettodb/lib" main.go
```

## API Reference

### Package Functions

```go
func Version() string                    // Get library version
func VersionNumber() int                 // Get version number
func Open(filename string) (*DB, error) // Open database
func CreateTable(name, schema string) (*Table, error) // Create table
func OpenTable(name string) (*Table, error)           // Open table
```

### Value Types

```go
func IntegerValue(val int64) Value       // Create integer value
func RealValue(val float64) Value        // Create real value
func TextValue(val string) Value         // Create text value
func NullValue() Value                   // Create null value
```

### DB (Original SQL API)

#### Methods
- `Close() error` - Close database connection
- `Exec(sql string) error` - Execute DDL/DML statements
- `Query(sql string) ([]QueryResult, error)` - Execute SELECT queries

#### Example
```go
db, err := ristretto.Open("mydb.db")
if err != nil {
    log.Fatal(err)
}
defer db.Close()

err = db.Exec("CREATE TABLE test (id INTEGER, name TEXT)")
if err != nil {
    log.Fatal(err)
}

err = db.Exec("INSERT INTO test VALUES (1, 'Hello')")
if err != nil {
    log.Fatal(err)
}

results, err := db.Query("SELECT * FROM test")
if err != nil {
    log.Fatal(err)
}

for _, row := range results {
    fmt.Printf("ID: %s, Name: %s\n", row["id"], row["name"])
}
```

### Table (Table V2 Ultra-Fast API)

#### Methods
- `Close() error` - Close table
- `AppendRow(values []Value) error` - High-speed row insertion
- `GetRowCount() int64` - Get total number of rows
- `Name() string` - Get table name

#### Example
```go
table, err := ristretto.CreateTable("logs", 
    "CREATE TABLE logs (timestamp INTEGER, message TEXT(64))")
if err != nil {
    log.Fatal(err)
}
defer table.Close()

values := []ristretto.Value{
    ristretto.IntegerValue(time.Now().Unix()),
    ristretto.TextValue("Application started"),
}

err = table.AppendRow(values)
if err != nil {
    log.Fatal(err)
}

fmt.Printf("Total logs: %d\n", table.GetRowCount())
```

### Error Handling

```go
type RistrettoError struct {
    Code    Result
    Message string
}

func (e *RistrettoError) Error() string

// Usage
db, err := ristretto.Open("mydb.db")
if err != nil {
    if ristrettoErr, ok := err.(*ristretto.RistrettoError); ok {
        fmt.Printf("RistrettoDB error code: %s\n", ristrettoErr.Code.String())
    }
    log.Fatal(err)
}
```

## Use Cases

### High-Performance Web API
```go
package main

import (
    "encoding/json"
    "log"
    "net/http"
    "strconv"
    "time"
    
    "./ristretto"
)

var db *ristretto.DB

func init() {
    var err error
    db, err = ristretto.Open("api.db")
    if err != nil {
        log.Fatal(err)
    }
    
    // Initialize API logs table
    err = db.Exec(`
        CREATE TABLE api_requests (
            timestamp INTEGER,
            method TEXT,
            path TEXT,
            status_code INTEGER,
            response_time_ms INTEGER
        )
    `)
    if err != nil {
        log.Printf("Table might already exist: %v", err)
    }
}

func logRequest(method, path string, statusCode int, responseTime time.Duration) {
    err := db.Exec(fmt.Sprintf(`
        INSERT INTO api_requests VALUES (%d, '%s', '%s', %d, %d)
    `, time.Now().Unix(), method, path, statusCode, responseTime.Milliseconds()))
    
    if err != nil {
        log.Printf("Failed to log request: %v", err)
    }
}

func statsHandler(w http.ResponseWriter, r *http.Request) {
    start := time.Now()
    
    results, err := db.Query(`
        SELECT 
            method,
            COUNT(*) as request_count,
            AVG(response_time_ms) as avg_response_time
        FROM api_requests 
        WHERE timestamp > ?
        GROUP BY method
    `, time.Now().Add(-24*time.Hour).Unix())
    
    if err != nil {
        http.Error(w, err.Error(), http.StatusInternalServerError)
        logRequest(r.Method, r.URL.Path, 500, time.Since(start))
        return
    }
    
    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(results)
    
    logRequest(r.Method, r.URL.Path, 200, time.Since(start))
}

func main() {
    defer db.Close()
    
    http.HandleFunc("/api/stats", statsHandler)
    
    log.Println("Starting API server on :8080")
    log.Fatal(http.ListenAndServe(":8080", nil))
}
```

### IoT Data Collection System
```go
package main

import (
    "fmt"
    "log"
    "math/rand"
    "time"
    
    "./ristretto"
)

type SensorReading struct {
    DeviceID    int64
    Temperature float64
    Humidity    float64
    BatteryLevel float64
    Location    string
}

func main() {
    // Create ultra-fast sensor data table
    table, err := ristretto.CreateTable("sensor_data", `
        CREATE TABLE sensor_data (
            timestamp INTEGER,
            device_id INTEGER,
            temperature REAL,
            humidity REAL,
            battery_level REAL,
            location TEXT(16)
        )
    `)
    if err != nil {
        log.Fatal(err)
    }
    defer table.Close()
    
    fmt.Println("SENSORS: Starting IoT data collection...")
    
    locations := []string{"Kitchen", "Bedroom", "Garage", "Attic", "Basement"}
    
    // Simulate continuous sensor data ingestion
    for i := 0; i < 1000; i++ {
        reading := SensorReading{
            DeviceID:     int64(rand.Intn(10)),
            Temperature:  20.0 + rand.Float64()*25.0, // 20-45°C
            Humidity:     40.0 + rand.Float64()*40.0, // 40-80%
            BatteryLevel: rand.Float64() * 100.0,     // 0-100%
            Location:     locations[rand.Intn(len(locations))],
        }
        
        values := []ristretto.Value{
            ristretto.IntegerValue(time.Now().Unix()),
            ristretto.IntegerValue(reading.DeviceID),
            ristretto.RealValue(reading.Temperature),
            ristretto.RealValue(reading.Humidity),
            ristretto.RealValue(reading.BatteryLevel),
            ristretto.TextValue(reading.Location),
        }
        
        err = table.AppendRow(values)
        if err != nil {
            log.Printf("Failed to insert reading: %v", err)
            continue
        }
        
        if i%100 == 0 {
            fmt.Printf("Processed %d readings, total: %d\n", i, table.GetRowCount())
        }
        
        // Simulate realistic sensor timing
        time.Sleep(10 * time.Millisecond)
    }
    
    fmt.Printf("SUCCESS: Collection complete. Total readings: %d\n", table.GetRowCount())
}
```

### Microservice Event Logging
```go
package main

import (
    "context"
    "fmt"
    "log"
    "os"
    "os/signal"
    "syscall"
    "time"
    
    "./ristretto"
)

type EventLogger struct {
    table  *ristretto.Table
    events chan Event
}

type Event struct {
    ServiceName string
    EventType   string
    Message     string
    Severity    string
}

func NewEventLogger() (*EventLogger, error) {
    table, err := ristretto.CreateTable("service_events", `
        CREATE TABLE service_events (
            timestamp INTEGER,
            service_name TEXT(32),
            event_type TEXT(16),
            message TEXT(128),
            severity TEXT(8)
        )
    `)
    if err != nil {
        return nil, err
    }
    
    logger := &EventLogger{
        table:  table,
        events: make(chan Event, 1000),
    }
    
    // Start event processing goroutine
    go logger.processEvents()
    
    return logger, nil
}

func (el *EventLogger) LogEvent(event Event) {
    select {
    case el.events <- event:
    default:
        log.Println("Event buffer full, dropping event")
    }
}

func (el *EventLogger) processEvents() {
    for event := range el.events {
        values := []ristretto.Value{
            ristretto.IntegerValue(time.Now().Unix()),
            ristretto.TextValue(event.ServiceName),
            ristretto.TextValue(event.EventType),
            ristretto.TextValue(event.Message),
            ristretto.TextValue(event.Severity),
        }
        
        err := el.table.AppendRow(values)
        if err != nil {
            log.Printf("Failed to log event: %v", err)
        }
    }
}

func (el *EventLogger) Close() {
    close(el.events)
    el.table.Close()
}

func main() {
    logger, err := NewEventLogger()
    if err != nil {
        log.Fatal(err)
    }
    defer logger.Close()
    
    // Simulate microservice events
    services := []string{"auth-service", "user-service", "order-service", "payment-service"}
    eventTypes := []string{"request", "response", "error", "warning", "info"}
    severities := []string{"info", "warning", "error", "critical"}
    
    // Log events
    go func() {
        for i := 0; i < 500; i++ {
            event := Event{
                ServiceName: services[i%len(services)],
                EventType:   eventTypes[i%len(eventTypes)],
                Message:     fmt.Sprintf("Event message %d", i),
                Severity:    severities[i%len(severities)],
            }
            
            logger.LogEvent(event)
            time.Sleep(50 * time.Millisecond)
        }
    }()
    
    // Wait for shutdown signal
    sigChan := make(chan os.Signal, 1)
    signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
    
    fmt.Println("SERVICE: Event logging service started. Press Ctrl+C to stop.")
    <-sigChan
    
    fmt.Printf("STATS: Total events logged: %d\n", logger.table.GetRowCount())
    fmt.Println("SHUTDOWN: Shutting down event logger...")
}
```

## Performance Tips

1. **Use Table V2 for Write-Heavy Workloads**: 4.57x faster than SQLite
2. **Batch Operations**: Group multiple operations when possible
3. **Resource Management**: Always use `defer Close()` 
4. **Concurrent Access**: RistrettoDB handles thread safety internally
5. **Memory Efficiency**: Fixed-width rows and memory-mapped I/O

## Architecture

The Go bindings use `cgo` to interface with the RistrettoDB C library:

```
Go Application
       ↓
  Go Bindings (ristretto.go)
       ↓ cgo
  RistrettoDB C Library (libristretto.so)
       ↓
  Memory-Mapped Files
```

## Requirements

- **Go**: 1.19+
- **CGO**: Enabled (default)
- **System**: POSIX-compliant (Linux, macOS, BSD)
- **RistrettoDB**: Library built (`make lib`)

## Build Configuration

### Environment Variables
```bash
export CGO_LDFLAGS="-L/path/to/ristrettodb/lib"
export LD_LIBRARY_PATH="/path/to/ristrettodb/lib:$LD_LIBRARY_PATH"
```

### Build Commands
```bash
# Simple build
go build example.go

# Build with library path
go build -ldflags "-L../../lib" example.go

# Cross-compilation (ensure library is available for target)
GOOS=linux GOARCH=amd64 go build example.go
```

## Thread Safety

RistrettoDB Go bindings are thread-safe:

- Each database/table instance uses mutexes for protection
- Safe for concurrent access from multiple goroutines
- Automatic resource cleanup with finalizers
- No global state in the bindings

## Troubleshooting

### Library Not Found
```
ld: library not found for -lristretto
```
**Solution**: Set CGO_LDFLAGS and build library:
```bash
cd ../../ && make lib
export CGO_LDFLAGS="-L$(pwd)/lib"
```

### CGO Disabled
```
cgo: C compiler not available
```
**Solution**: Enable CGO:
```bash
export CGO_ENABLED=1
```

### Runtime Library Error
```
error while loading shared libraries: libristretto.so
```
**Solution**: Set library path:
```bash
export LD_LIBRARY_PATH="/path/to/lib:$LD_LIBRARY_PATH"
```

### Header Not Found
```
fatal error: 'ristretto.h' file not found
```
**Solution**: Ensure header is in embed directory and CGO can find it.

## Production Deployment

### Docker Example
```dockerfile
FROM golang:1.19-alpine AS builder

# Install build dependencies
RUN apk add --no-cache gcc musl-dev

WORKDIR /app

# Copy RistrettoDB library and headers
COPY lib/ ./lib/
COPY embed/ristretto.h ./embed/

# Set CGO flags
ENV CGO_LDFLAGS="-L./lib"
ENV CGO_CFLAGS="-I./embed"

# Copy Go source
COPY go.mod go.sum ./
RUN go mod download

COPY . .
RUN go build -o app main.go

# Runtime stage
FROM alpine:latest
RUN apk --no-cache add ca-certificates libc6-compat
WORKDIR /root/

# Copy binary and library
COPY --from=builder /app/app .
COPY --from=builder /app/lib/libristretto.so /usr/local/lib/

# Set library path
ENV LD_LIBRARY_PATH=/usr/local/lib

CMD ["./app"]
```

### Kubernetes Deployment
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: ristretto-app
spec:
  replicas: 3
  selector:
    matchLabels:
      app: ristretto-app
  template:
    metadata:
      labels:
        app: ristretto-app
    spec:
      containers:
      - name: app
        image: your-registry/ristretto-app:latest
        env:
        - name: LD_LIBRARY_PATH
          value: "/usr/local/lib"
        volumeMounts:
        - name: data
          mountPath: /data
      volumes:
      - name: data
        persistentVolumeClaim:
          claimName: ristretto-data
```

## Contributing

Found a bug or want to improve the Go bindings? Please open an issue or submit a pull request at the main RistrettoDB repository.

## License

MIT License - same as RistrettoDB core library.