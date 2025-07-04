# RistrettoDB Node.js Bindings

High-performance Node.js bindings for RistrettoDB, the tiny, blazingly fast, embeddable SQL engine.

## Features

- **Original SQL API**: 2.8x faster than SQLite for general SQL operations
- **Table V2 Ultra-Fast API**: 4.57x faster than SQLite for append-only workloads
- **Async/Await Compatible**: Non-blocking I/O friendly
- **Zero Dependencies**: Uses only ffi-napi and ref-napi for C interop
- **Memory Efficient**: Direct memory-mapped file access
- **Error Handling**: Comprehensive error handling with detailed messages

## Performance

| API | Use Case | Performance vs SQLite |
|-----|----------|----------------------|
| **Original SQL** | Web APIs, general SQL | **2.8x faster** |
| **Table V2** | Analytics, IoT, logging | **4.57x faster** |

## Quick Start

### 1. Build RistrettoDB Library

```bash
# From the RistrettoDB root directory
cd ../../
make lib
```

### 2. Install Dependencies

```bash
npm install
```

### 3. Run the Example

```bash
node example.js
```

### 4. Use in Your Project

```javascript
const { RistrettoDB, RistrettoTable, RistrettoValue } = require('./ristretto');

// Original SQL API - General Purpose
const db = new RistrettoDB('myapp.db');
db.exec('CREATE TABLE users (id INTEGER, name TEXT, score REAL)');
db.exec("INSERT INTO users VALUES (1, 'Alice', 95.5)");

const results = db.query('SELECT * FROM users WHERE score > 90');
results.forEach(row => {
  console.log(`User: ${row.name}, Score: ${row.score}`);
});
db.close();

// Table V2 API - Ultra-Fast Writes
const table = RistrettoTable.create('events', 
  'CREATE TABLE events (timestamp INTEGER, event TEXT(32))');

table.appendRow([
  RistrettoValue.integer(Date.now()),
  RistrettoValue.text('user_login')
]);

console.log(`Total events: ${table.getRowCount()}`);
table.close();
```

## Installation

### Dependencies

```bash
npm install ffi-napi ref-napi
```

### Copy Bindings

```bash
# Copy to your project
cp examples/nodejs/ristretto.js your_project/
```

## API Reference

### RistrettoDB (Original SQL API)

#### Constructor
```javascript
const db = new RistrettoDB(filename)
```

#### Methods
- `exec(sql)` - Execute DDL/DML statements
- `query(sql, callback?)` - Execute SELECT queries, returns array of objects
- `close()` - Close database connection
- `RistrettoDB.version()` - Get library version (static method)

#### Example
```javascript
const db = new RistrettoDB('mydb.db');
db.exec('CREATE TABLE test (id INTEGER, name TEXT)');
db.exec("INSERT INTO test VALUES (1, 'Hello')");

const results = db.query('SELECT * FROM test');
console.log(results); // [{ id: '1', name: 'Hello' }]

db.close();
```

### RistrettoTable (Table V2 Ultra-Fast API)

#### Creation/Opening
```javascript
const table = RistrettoTable.create(name, schemaSql)
const table = RistrettoTable.open(name)
```

#### Methods
- `appendRow(values)` - High-speed row insertion
- `getRowCount()` - Get total number of rows
- `close()` - Close table

#### Example
```javascript
const table = RistrettoTable.create('logs', 
  'CREATE TABLE logs (timestamp INTEGER, message TEXT(64))');

table.appendRow([
  RistrettoValue.integer(Date.now()),
  RistrettoValue.text('Application started')
]);

console.log(`Total logs: ${table.getRowCount()}`);
table.close();
```

### RistrettoValue (Value Types)

#### Factory Methods
```javascript
RistrettoValue.integer(value)    // Create integer value
RistrettoValue.real(value)       // Create real/float value  
RistrettoValue.text(value)       // Create text value
RistrettoValue.null()            // Create null value
```

#### Properties
- `type` - Column type (INTEGER, REAL, TEXT, NULLABLE)
- `value` - The actual value
- `isNull` - Whether value is null

## Error Handling

```javascript
const { RistrettoError } = require('./ristretto');

try {
  const db = new RistrettoDB('mydb.db');
  db.exec('INVALID SQL');
} catch (error) {
  if (error instanceof RistrettoError) {
    console.error(`Database error: ${error.message}`);
    console.error(`Error code: ${error.resultCode}`);
  }
}
```

## Use Cases

### Real-time Web Analytics
```javascript
const { RistrettoTable, RistrettoValue } = require('./ristretto');

// High-speed analytics event ingestion
const analytics = RistrettoTable.create('events', `
  CREATE TABLE events (
    timestamp INTEGER,
    user_id INTEGER,
    event_type TEXT(16),
    page_url TEXT(64)
  )
`);

// Log user events with minimal overhead
app.post('/api/events', (req, res) => {
  analytics.appendRow([
    RistrettoValue.integer(Date.now()),
    RistrettoValue.integer(req.body.userId),
    RistrettoValue.text(req.body.eventType),
    RistrettoValue.text(req.body.pageUrl)
  ]);
  
  res.json({ status: 'logged' });
});
```

### Express.js API with High-speed Logging
```javascript
const express = require('express');
const { RistrettoDB } = require('./ristretto');

const app = express();
const db = new RistrettoDB('api.db');

// Initialize database
db.exec(`
  CREATE TABLE api_logs (
    timestamp INTEGER,
    method TEXT,
    path TEXT,
    status_code INTEGER,
    response_time REAL
  )
`);

// Logging middleware
app.use((req, res, next) => {
  const start = Date.now();
  
  res.on('finish', () => {
    const responseTime = Date.now() - start;
    
    db.exec(`
      INSERT INTO api_logs VALUES (
        ${Date.now()},
        '${req.method}',
        '${req.path}',
        ${res.statusCode},
        ${responseTime}
      )
    `);
  });
  
  next();
});

app.get('/api/stats', (req, res) => {
  const stats = db.query(`
    SELECT 
      method,
      COUNT(*) as requests,
      AVG(response_time) as avg_response_time
    FROM api_logs 
    WHERE timestamp > ${Date.now() - 24 * 60 * 60 * 1000}
    GROUP BY method
  `);
  
  res.json(stats);
});
```

### IoT Data Collection
```javascript
const { RistrettoTable, RistrettoValue } = require('./ristretto');

// Ultra-fast sensor data table
const sensors = RistrettoTable.create('sensor_data', `
  CREATE TABLE sensor_data (
    timestamp INTEGER,
    device_id INTEGER,
    temperature REAL,
    humidity REAL,
    battery_level REAL
  )
`);

// WebSocket server for real-time sensor data
const WebSocket = require('ws');
const wss = new WebSocket.Server({ port: 8080 });

wss.on('connection', (ws) => {
  ws.on('message', (data) => {
    const sensorReading = JSON.parse(data);
    
    // High-speed data ingestion
    sensors.appendRow([
      RistrettoValue.integer(Date.now()),
      RistrettoValue.integer(sensorReading.deviceId),
      RistrettoValue.real(sensorReading.temperature),
      RistrettoValue.real(sensorReading.humidity),
      RistrettoValue.real(sensorReading.batteryLevel)
    ]);
  });
});
```

## Performance Tips

1. **Use Table V2 for Write-Heavy Workloads**: 4.57x faster than SQLite
2. **Batch Operations**: Group multiple operations when possible
3. **Proper Error Handling**: Always use try-catch blocks
4. **Resource Cleanup**: Always call `close()` methods
5. **Memory Management**: RistrettoDB handles memory-mapped I/O efficiently

## Architecture

The Node.js bindings use `ffi-napi` to interface with the RistrettoDB C library:

```
Node.js Application
       ↓
  Node.js Bindings (ristretto.js)
       ↓ ffi-napi
  RistrettoDB C Library (libristretto.so)
       ↓
  Memory-Mapped Files
```

## Requirements

- **Node.js**: 12.0.0+
- **Dependencies**: ffi-napi, ref-napi
- **System**: POSIX-compliant (Linux, macOS, BSD)
- **RistrettoDB**: Library built (`make lib`)

## Dependencies

The bindings require these Node.js packages:

```json
{
  "dependencies": {
    "ffi-napi": "^4.0.3",
    "ref-napi": "^3.0.3"
  }
}
```

Install with:
```bash
npm install ffi-napi ref-napi
```

## Thread Safety

RistrettoDB is currently single-threaded. For multi-threaded Node.js applications:

1. Use separate database instances per worker thread
2. Implement application-level coordination if sharing databases
3. Consider using separate files for different processes

## Troubleshooting

### Library Not Found
```
Error: Could not find libristretto.so
```
**Solution**: Build the library first:
```bash
cd ../../ && make lib
```

### ffi-napi Installation Issues
```
Error: Cannot find module 'ffi-napi'
```
**Solution**: Install dependencies:
```bash
npm install ffi-napi ref-napi
```

### Permission Denied
```
Error: EACCES: permission denied
```
**Solution**: Check file permissions or run with appropriate privileges

### Node.js Version Issues
```
Error: Unsupported Node.js version
```
**Solution**: Use Node.js 12.0.0 or higher

## Production Deployment

### Docker Example
```dockerfile
FROM node:16-alpine

# Install build dependencies for native modules
RUN apk add --no-cache python3 make g++

WORKDIR /app

# Copy RistrettoDB library
COPY lib/libristretto.so /usr/local/lib/
COPY examples/nodejs/ristretto.js ./

# Install Node.js dependencies
COPY package*.json ./
RUN npm ci --only=production

# Copy application
COPY . .

CMD ["node", "app.js"]
```

### PM2 Configuration
```json
{
  "apps": [{
    "name": "ristretto-app",
    "script": "app.js",
    "instances": 1,
    "env": {
      "NODE_ENV": "production",
      "LD_LIBRARY_PATH": "/usr/local/lib"
    }
  }]
}
```

## Contributing

Found a bug or want to improve the Node.js bindings? Please open an issue or submit a pull request at the main RistrettoDB repository.

## License

MIT License - same as RistrettoDB core library.