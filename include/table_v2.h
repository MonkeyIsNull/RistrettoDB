#ifndef RISTRETTO_TABLE_V2_H
#define RISTRETTO_TABLE_V2_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/mman.h>

#define MAX_COLUMNS 14
#define MAX_COLUMN_NAME 32
// Header region reserved at the start of every .rdb file. Must be >= the
// actual sizeof(TableHeader) (checked by the _Static_assert below) so that
// row data written at TABLE_HEADER_SIZE never overlaps the column-descriptor
// array. 1024 leaves generous headroom and is page-friendly.
#define TABLE_HEADER_SIZE 1024
#define INITIAL_FILE_SIZE (1024 * 1024)  // 1 MB initial size
#define GROWTH_FACTOR 2                   // Double size when growing
#define SYNC_INTERVAL_ROWS 512           // Sync every N rows
#define SYNC_INTERVAL_MS 100             // Sync every N milliseconds

// Magic bytes for file format identification
#define TABLE_MAGIC "RSTRDB\x00\x00"
// Format version. Bumped 1 -> 2 when TABLE_HEADER_SIZE grew from 256 to 1024:
// the row-data offset moved, so version-1 files are cleanly rejected on open.
#define TABLE_VERSION 2

// Open modes for table_create_ex / table_open_ex.
typedef enum {
    RDB_CREATE_NEW = 0,          // Create; fail (O_EXCL) if the file exists
    RDB_CREATE_OR_TRUNCATE = 1,  // Create or truncate existing (legacy default)
    RDB_OPEN_OR_CREATE = 2       // Open existing, or create if absent
} RdbOpenMode;

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
    ColumnDesc columns[MAX_COLUMNS];  // Column descriptors
} TableHeader;

// Row data must never overlap the header region. If this fails, raise
// TABLE_HEADER_SIZE (and bump TABLE_VERSION since the on-disk layout changes).
_Static_assert(TABLE_HEADER_SIZE >= sizeof(TableHeader),
               "row data must not overlap the table header");

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

// Extended lifecycle: choose the storage directory (base_dir, NULL = "data")
// and, for create, the open mode (RDB_CREATE_NEW / _OR_TRUNCATE / OPEN_OR_CREATE).
// The two-argument table_create/table_open above are thin wrappers over these
// with base_dir="data" and open_mode=RDB_CREATE_OR_TRUNCATE.
Table* table_create_ex(const char *name, const char *schema_sql,
                       const char *base_dir, int open_mode);
Table* table_open_ex(const char *name, const char *base_dir);

// Core operations
// table_append_row trusts that values[] holds exactly header->column_count
// entries. table_append_row_n validates the count first (recommended for
// language bindings and untrusted callers).
bool table_append_row(Table *table, const Value *values);
bool table_append_row_n(Table *table, const Value *values, uint32_t value_count);
bool table_select(Table *table, const char *where_clause,
                 void (*callback)(void *ctx, const Value *row), void *ctx);

// File management
bool table_flush(Table *table);         // MS_ASYNC (fast, not durable)
bool table_flush_durable(Table *table); // MS_SYNC + fsync (durable)
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
bool create_data_directory(void);               // creates "data" in the CWD
bool create_data_directory_in(const char *base_dir); // creates base_dir (NULL = "data")

#endif
