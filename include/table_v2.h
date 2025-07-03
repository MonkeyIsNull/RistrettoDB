#ifndef RISTRETTO_TABLE_V2_H
#define RISTRETTO_TABLE_V2_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/mman.h>

#define MAX_COLUMNS 14
#define MAX_COLUMN_NAME 8
#define TABLE_HEADER_SIZE 256
#define INITIAL_FILE_SIZE (1024 * 1024)  // 1 MB initial size
#define GROWTH_FACTOR 2                   // Double size when growing
#define SYNC_INTERVAL_ROWS 512           // Sync every N rows
#define SYNC_INTERVAL_MS 100             // Sync every N milliseconds

// Magic bytes for file format identification
#define TABLE_MAGIC "RSTRDB\x00\x00"
#define TABLE_VERSION 1

typedef enum {
    COL_TYPE_INTEGER = 1,
    COL_TYPE_REAL = 2,
    COL_TYPE_TEXT = 3,
    COL_TYPE_NULLABLE = 4
} ColumnType;

typedef struct {
    char name[MAX_COLUMN_NAME];  // Column name (truncated/padded)
    uint8_t type;                // ColumnType
    uint8_t length;              // Bytes if TEXT, 0 for INTEGER/REAL
    uint16_t offset;             // Byte offset within row
    uint8_t reserved[4];         // Padding/reserved for future use
} ColumnDesc;

typedef struct {
    char magic[8];               // "RSTRDB\x00\x00"
    uint32_t version;            // File format version
    uint32_t row_size;           // Size in bytes of a single row
    uint64_t num_rows;           // Number of rows written
    uint32_t column_count;       // Number of columns
    uint8_t reserved[12];        // Reserved for future use
    ColumnDesc columns[MAX_COLUMNS];  // Column descriptors (224 bytes)
} TableHeader;

typedef struct {
    char name[64];               // Table name
    int fd;                      // File descriptor
    uint8_t *mapped_ptr;         // Memory-mapped file pointer
    size_t mapped_size;          // Current mapped size
    size_t write_offset;         // Current write position
    TableHeader *header;         // Pointer to header in mapped memory
    
    // Performance tracking
    uint64_t rows_since_sync;    // Rows written since last sync
    uint64_t last_sync_time_ms;  // Last sync timestamp
    
    // File path for remapping
    char file_path[256];
} Table;

typedef struct {
    ColumnType type;
    union {
        int64_t integer;
        double real;
        struct {
            char *data;
            size_t length;
        } text;
    } value;
    bool is_null;
} Value;

// Table lifecycle functions
Table* table_create(const char *name, const char *schema_sql);
Table* table_open(const char *name);
void table_close(Table *table);

// Core operations
bool table_append_row(Table *table, const Value *values);
bool table_select(Table *table, const char *where_clause, 
                 void (*callback)(void *ctx, const Value *row), void *ctx);

// File management
bool table_flush(Table *table);
bool table_remap(Table *table);
bool table_ensure_space(Table *table, size_t needed_bytes);

// Schema and metadata
bool table_parse_schema(const char *schema_sql, ColumnDesc *columns, 
                       uint32_t *column_count, uint32_t *row_size);
const ColumnDesc* table_get_column(Table *table, const char *name);
size_t table_get_row_count(Table *table);

// Value utilities
Value value_integer(int64_t val);
Value value_real(double val);
Value value_text(const char *str);
Value value_null(void);
void value_destroy(Value *value);

// Row packing/unpacking
bool table_pack_row(Table *table, const Value *values, uint8_t *row_buffer);
bool table_unpack_row(Table *table, const uint8_t *row_buffer, Value *values);

// Utility functions
uint64_t get_time_ms(void);
bool create_data_directory(void);

#endif
