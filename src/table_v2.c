#include "table_v2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>   // flock
#include <time.h>
#include <errno.h>

// Utility functions
uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

bool create_data_directory_in(const char *base_dir) {
    if (!base_dir) base_dir = "data";
    struct stat st = {0};
    if (stat(base_dir, &st) == -1) {
        if (mkdir(base_dir, 0755) == 0) return true;
        // Tolerate a concurrent creator.
        return errno == EEXIST;
    }
    return true;
}

bool create_data_directory(void) {
    return create_data_directory_in("data");
}

// Value constructors
Value value_integer(int64_t val) {
    Value v = {0};
    v.type = COL_TYPE_INTEGER;
    v.value.integer = val;
    v.is_null = false;
    return v;
}

Value value_real(double val) {
    Value v = {0};
    v.type = COL_TYPE_REAL;
    v.value.real = val;
    v.is_null = false;
    return v;
}

Value value_text(const char *str) {
    Value v = {0};
    v.type = COL_TYPE_TEXT;
    if (str) {
        size_t len = strlen(str);
        v.value.text.data = malloc(len + 1);
        if (v.value.text.data) {
            strcpy(v.value.text.data, str);
            v.value.text.length = len;
        }
    }
    v.is_null = (str == NULL);
    return v;
}

Value value_null(void) {
    Value v = {0};
    v.is_null = true;
    return v;
}

void value_destroy(Value *value) {
    if (value && value->type == COL_TYPE_TEXT && value->value.text.data) {
        free(value->value.text.data);
        value->value.text.data = NULL;
        value->value.text.length = 0;
    }
}

// Schema parsing (simplified - will need full parser integration later)
bool table_parse_schema(const char *schema_sql, ColumnDesc *columns, 
                       uint32_t *column_count, uint32_t *row_size) {
    // Simple parser for basic CREATE TABLE statements
    // Format: CREATE TABLE name (col1 TYPE, col2 TYPE(size), ...)
    
    *column_count = 0;
    *row_size = 0;
    
    // Find the opening parenthesis
    const char *start = strchr(schema_sql, '(');
    if (!start) return false;
    start++;
    
    const char *end = strrchr(schema_sql, ')');
    if (!end) return false;
    
    // Parse column definitions
    char col_def[256];
    const char *current = start;
    uint32_t offset = 0;
    
    while (current < end && *column_count < MAX_COLUMNS) {
        // Extract column definition
        const char *comma = strchr(current, ',');
        if (!comma) comma = end;
        
        size_t def_len = comma - current;
        if (def_len >= sizeof(col_def)) def_len = sizeof(col_def) - 1;
        
        strncpy(col_def, current, def_len);
        col_def[def_len] = '\0';
        
        // Parse column name and type
        char name[MAX_COLUMN_NAME] = {0};
        char type_str[32] = {0};
        int length = 0;
        
        if (sscanf(col_def, " %31s %31s", name, type_str) < 2) {
            break;
        }
        
        // Determine column type and size
        ColumnDesc *col = &columns[*column_count];
        strncpy(col->name, name, MAX_COLUMN_NAME - 1);
        col->offset = offset;
        
        if (strncmp(type_str, "INTEGER", 7) == 0) {
            col->type = COL_TYPE_INTEGER;
            col->length = 8;
        } else if (strncmp(type_str, "REAL", 4) == 0) {
            col->type = COL_TYPE_REAL;
            col->length = 8;
        } else if (strncmp(type_str, "TEXT", 4) == 0) {
            col->type = COL_TYPE_TEXT;
            // Parse TEXT(n) format
            if (sscanf(type_str, "TEXT(%d)", &length) == 1) {
                col->length = (length > 255) ? 255 : length;
            } else {
                col->length = 64; // Default text size
            }
        } else {
            return false; // Unsupported type
        }
        
        offset += col->length;
        (*column_count)++;
        
        current = comma + 1;
        if (comma == end) break;
    }
    
    *row_size = offset;
    return *column_count > 0;
}

// Table creation (extended)
Table* table_create_ex(const char *name, const char *schema_sql,
                       const char *base_dir, int open_mode) {
    if (!name || !schema_sql) return NULL;
    if (!base_dir) base_dir = "data";

    if (!create_data_directory_in(base_dir)) {
        return NULL;
    }

    Table *table = calloc(1, sizeof(Table));
    if (!table) return NULL;

    // Parse schema into temporary variables
    ColumnDesc temp_columns[MAX_COLUMNS];
    uint32_t temp_column_count, temp_row_size;
    if (!table_parse_schema(schema_sql, temp_columns, &temp_column_count, &temp_row_size)) {
        free(table);
        return NULL;
    }

    // Create file path
    snprintf(table->file_path, sizeof(table->file_path), "%s/%s.rdb", base_dir, name);
    strncpy(table->name, name, sizeof(table->name) - 1);

    // RDB_OPEN_OR_CREATE: resume an existing table rather than recreating it.
    if (open_mode == RDB_OPEN_OR_CREATE) {
        struct stat existing;
        if (stat(table->file_path, &existing) == 0 &&
            existing.st_size >= (off_t)TABLE_HEADER_SIZE) {
            free(table);
            return table_open_ex(name, base_dir);
        }
    }

    // Select open flags based on the requested mode.
    int flags = O_CREAT | O_RDWR;
    switch (open_mode) {
        case RDB_CREATE_NEW:         flags |= O_EXCL;  break; // fail if exists
        case RDB_OPEN_OR_CREATE:     /* no O_TRUNC, no O_EXCL */ break;
        case RDB_CREATE_OR_TRUNCATE:
        default:                     flags |= O_TRUNC; break;
    }

    // Create and open file
    table->fd = open(table->file_path, flags, 0644);
    if (table->fd == -1) {
        if (open_mode == RDB_CREATE_NEW && errno == EEXIST) {
            fprintf(stderr, "table_create: '%s' already exists (RDB_CREATE_NEW)\n",
                    table->file_path);
        }
        free(table);
        return NULL;
    }

    // Advisory single-writer lock. NB: advisory only, and a no-op on some
    // network filesystems (NFS/SMB); released implicitly on close(fd).
    if (flock(table->fd, LOCK_EX | LOCK_NB) == -1) {
        if (errno == EWOULDBLOCK) {
            fprintf(stderr, "table_create: '%s' locked by another process\n",
                    table->file_path);
        }
        close(table->fd);
        free(table);
        return NULL;
    }

    // Set initial file size
    if (ftruncate(table->fd, INITIAL_FILE_SIZE) == -1) {
        close(table->fd);
        free(table);
        return NULL;
    }
    
    // Memory map the file
    table->mapped_ptr = mmap(NULL, INITIAL_FILE_SIZE, PROT_READ | PROT_WRITE, 
                            MAP_SHARED, table->fd, 0);
    if (table->mapped_ptr == MAP_FAILED) {
        close(table->fd);
        free(table);
        return NULL;
    }
    
    table->mapped_size = INITIAL_FILE_SIZE;
    table->header = (TableHeader*)table->mapped_ptr;
    table->write_offset = TABLE_HEADER_SIZE;
    
    // Initialize header
    memcpy(table->header->magic, TABLE_MAGIC, 8);
    table->header->version = TABLE_VERSION;
    table->header->num_rows = 0;
    
    // Copy parsed schema to header
    table->header->column_count = temp_column_count;
    table->header->row_size = temp_row_size;
    memcpy(table->header->columns, temp_columns, sizeof(ColumnDesc) * temp_column_count);
    
    table->rows_since_sync = 0;
    table->last_sync_time_ms = get_time_ms();

    return table;
}

// Table creation. Convenience wrapper: stores under "data/" and TRUNCATES any
// existing file. For non-destructive creation use table_create_ex with
// RDB_CREATE_NEW, and pass a base_dir to control the storage location.
Table* table_create(const char *name, const char *schema_sql) {
    return table_create_ex(name, schema_sql, "data", RDB_CREATE_OR_TRUNCATE);
}

// Table opening (extended)
Table* table_open_ex(const char *name, const char *base_dir) {
    if (!name) return NULL;
    if (!base_dir) base_dir = "data";

    Table *table = calloc(1, sizeof(Table));
    if (!table) return NULL;

    // Create file path
    snprintf(table->file_path, sizeof(table->file_path), "%s/%s.rdb", base_dir, name);
    strncpy(table->name, name, sizeof(table->name) - 1);

    // Open existing file
    table->fd = open(table->file_path, O_RDWR);
    if (table->fd == -1) {
        free(table);
        return NULL;
    }

    // Advisory single-writer lock (see table_create_ex).
    if (flock(table->fd, LOCK_EX | LOCK_NB) == -1) {
        if (errno == EWOULDBLOCK) {
            fprintf(stderr, "table_open: '%s' locked by another process\n",
                    table->file_path);
        }
        close(table->fd);
        free(table);
        return NULL;
    }

    // Get file size
    struct stat st;
    if (fstat(table->fd, &st) == -1) {
        close(table->fd);
        free(table);
        return NULL;
    }
    
    if (st.st_size < TABLE_HEADER_SIZE) {
        close(table->fd);
        free(table);
        return NULL;
    }
    
    // Memory map the file
    table->mapped_ptr = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, 
                            MAP_SHARED, table->fd, 0);
    if (table->mapped_ptr == MAP_FAILED) {
        close(table->fd);
        free(table);
        return NULL;
    }
    
    table->mapped_size = st.st_size;
    table->header = (TableHeader*)table->mapped_ptr;
    
    // Validate header
    if (memcmp(table->header->magic, TABLE_MAGIC, 8) != 0 ||
        table->header->version != TABLE_VERSION) {
        munmap(table->mapped_ptr, table->mapped_size);
        close(table->fd);
        free(table);
        return NULL;
    }
    
    // Calculate write offset
    table->write_offset = TABLE_HEADER_SIZE + (table->header->num_rows * table->header->row_size);
    table->rows_since_sync = 0;
    table->last_sync_time_ms = get_time_ms();

    return table;
}

// Table opening. Convenience wrapper: resumes "data/<name>.rdb".
Table* table_open(const char *name) {
    return table_open_ex(name, "data");
}

// Table closing
void table_close(Table *table) {
    if (!table) return;

    // Durable flush at close so a reopen sees every committed row (covers the
    // header region, making num_rows durable). Runs once, before munmap.
    if (table->mapped_ptr && table->mapped_ptr != MAP_FAILED) {
        msync(table->mapped_ptr, table->write_offset, MS_SYNC);
    }
    if (table->fd != -1) {
        fsync(table->fd);
    }

    if (table->mapped_ptr && table->mapped_ptr != MAP_FAILED) {
        munmap(table->mapped_ptr, table->mapped_size);
    }

    if (table->fd != -1) {
        close(table->fd);  // also releases the advisory flock
    }

    free(table);
}

// File growth and remapping
bool table_ensure_space(Table *table, size_t needed_bytes) {
    if (table->write_offset + needed_bytes <= table->mapped_size) {
        return true; // Already have enough space
    }
    
    return table_remap(table);
}

bool table_remap(Table *table) {
    size_t new_size = table->mapped_size * GROWTH_FACTOR;
    
    // Unmap current mapping
    if (munmap(table->mapped_ptr, table->mapped_size) == -1) {
        return false;
    }
    
    // Resize file
    if (ftruncate(table->fd, new_size) == -1) {
        return false;
    }
    
    // Remap with new size
    table->mapped_ptr = mmap(NULL, new_size, PROT_READ | PROT_WRITE, 
                            MAP_SHARED, table->fd, 0);
    if (table->mapped_ptr == MAP_FAILED) {
        return false;
    }
    
    table->mapped_size = new_size;
    table->header = (TableHeader*)table->mapped_ptr;
    
    return true;
}

// Row packing
bool table_pack_row(Table *table, const Value *values, uint8_t *row_buffer) {
    memset(row_buffer, 0, table->header->row_size);
    
    for (uint32_t i = 0; i < table->header->column_count; i++) {
        const ColumnDesc *col = &table->header->columns[i];
        const Value *val = &values[i];
        uint8_t *dest = row_buffer + col->offset;
        
        if (val->is_null) {
            // For now, just write zeros for NULL values
            continue;
        }
        
        switch (col->type) {
            case COL_TYPE_INTEGER:
                *(int64_t*)dest = val->value.integer;
                break;
                
            case COL_TYPE_REAL:
                *(double*)dest = val->value.real;
                break;
                
            case COL_TYPE_TEXT:
                // Guard zero-width TEXT: col->length is uint8_t, so col->length-1
                // would wrap to a huge size_t and defeat the clamp below.
                if (col->length == 0) {
                    break;
                }
                if (val->value.text.data) {
                    size_t copy_len = val->value.text.length;
                    if (copy_len > (size_t)(col->length - 1)) {
                        copy_len = (size_t)(col->length - 1);
                    }
                    memcpy(dest, val->value.text.data, copy_len);
                    dest[copy_len] = '\0';
                }
                break;
                
            default:
                return false;
        }
    }
    
    return true;
}

// Row unpacking
bool table_unpack_row(Table *table, const uint8_t *row_buffer, Value *values) {
    for (uint32_t i = 0; i < table->header->column_count; i++) {
        const ColumnDesc *col = &table->header->columns[i];
        const uint8_t *src = row_buffer + col->offset;
        Value *val = &values[i];
        
        val->type = col->type;
        val->is_null = false;
        
        switch (col->type) {
            case COL_TYPE_INTEGER:
                val->value.integer = *(int64_t*)src;
                break;
                
            case COL_TYPE_REAL:
                val->value.real = *(double*)src;
                break;
                
            case COL_TYPE_TEXT:
                val->value.text.length = strnlen((char*)src, col->length);
                val->value.text.data = malloc(val->value.text.length + 1);
                if (val->value.text.data) {
                    memcpy(val->value.text.data, src, val->value.text.length);
                    val->value.text.data[val->value.text.length] = '\0';
                } else {
                    return false;
                }
                break;
                
            default:
                return false;
        }
    }
    
    return true;
}

// Ultra-fast row insertion. Trusts that values[] holds exactly
// header->column_count entries; use table_append_row_n to validate the count.
bool table_append_row(Table *table, const Value *values) {
    if (!table || !values || table->header->column_count == 0) return false;

    // Ensure we have space for the row
    if (!table_ensure_space(table, table->header->row_size)) {
        return false;
    }
    
    // Pack row directly into mapped memory
    uint8_t *row_dest = table->mapped_ptr + table->write_offset;
    if (!table_pack_row(table, values, row_dest)) {
        return false;
    }
    
    // Update write position and row count
    table->write_offset += table->header->row_size;
    table->header->num_rows++;
    table->rows_since_sync++;
    
    // Check if we should sync
    uint64_t current_time = get_time_ms();
    if (table->rows_since_sync >= SYNC_INTERVAL_ROWS ||
        (current_time - table->last_sync_time_ms) >= SYNC_INTERVAL_MS) {
        table_flush(table);
    }
    
    return true;
}

// Count-checked row insertion. Rejects a values[] array whose length does not
// match the column count (prevents out-of-bounds reads in table_pack_row).
bool table_append_row_n(Table *table, const Value *values, uint32_t value_count) {
    if (!table || !values) return false;
    if (value_count != table->header->column_count) {
        fprintf(stderr, "table_append_row_n: expected %u values, got %u\n",
                table->header->column_count, value_count);
        return false;
    }
    return table_append_row(table, values);
}

// Sync and durability (fast, asynchronous).
bool table_flush(Table *table) {
    if (!table || !table->mapped_ptr) return false;

    // Sync memory-mapped region
    if (msync(table->mapped_ptr, table->write_offset, MS_ASYNC) == -1) {
        return false;
    }

    table->rows_since_sync = 0;
    table->last_sync_time_ms = get_time_ms();

    return true;
}

// Durable flush: synchronous msync + fsync. Use when the caller needs the data
// on stable storage; table_close also flushes durably.
bool table_flush_durable(Table *table) {
    if (!table || !table->mapped_ptr) return false;

    if (msync(table->mapped_ptr, table->write_offset, MS_SYNC) == -1) {
        return false;
    }
    if (table->fd != -1 && fsync(table->fd) == -1) {
        return false;
    }

    table->rows_since_sync = 0;
    table->last_sync_time_ms = get_time_ms();
    return true;
}

// Table scanning and selection
bool table_select(Table *table, const char *where_clause, 
                 void (*callback)(void *ctx, const Value *row), void *ctx) {
    if (!table || !callback) return false;
    
    (void)where_clause; // TODO: Implement WHERE clause parsing
    
    Value *row_values = malloc(sizeof(Value) * table->header->column_count);
    if (!row_values) return false;
    
    uint8_t *current_row = table->mapped_ptr + TABLE_HEADER_SIZE;
    
    for (uint64_t i = 0; i < table->header->num_rows; i++) {
        if (table_unpack_row(table, current_row, row_values)) {
            callback(ctx, row_values);
            
            // Clean up text values
            for (uint32_t j = 0; j < table->header->column_count; j++) {
                value_destroy(&row_values[j]);
            }
        }
        
        current_row += table->header->row_size;
    }
    
    free(row_values);
    return true;
}

// Utility functions
const ColumnDesc* table_get_column(Table *table, const char *name) {
    if (!table || !name) return NULL;
    
    for (uint32_t i = 0; i < table->header->column_count; i++) {
        if (strncmp(table->header->columns[i].name, name, MAX_COLUMN_NAME) == 0) {
            return &table->header->columns[i];
        }
    }
    
    return NULL;
}

size_t table_get_row_count(Table *table) {
    return table ? table->header->num_rows : 0;
}
