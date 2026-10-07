#!/usr/bin/env python3
"""
RistrettoDB Embedded Script

This script creates a single-file embedded of RistrettoDB source code,
similar to SQLite's embedded. The result is a single ristretto.c file
that can be easily embedded in projects.

Usage:
    python3 scripts/embed.py
    
Output:
    ristretto.c - Single source file containing all library code
    ristretto.h - Public header file (already exists)
"""

import os
import re
import sys
from pathlib import Path

def read_file(filepath):
    """Read file content with error handling"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            return f.read()
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return None

def write_file(filepath, content):
    """Write file content with error handling"""
    try:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Created: {filepath}")
        return True
    except Exception as e:
        print(f"Error writing {filepath}: {e}")
        return False

def process_includes(content, processed_files, base_dir):
    """
    Process #include directives, replacing local includes with actual content
    """
    lines = content.split('\n')
    result_lines = []
    
    for line in lines:
        # Match #include "filename.h" (local includes)
        local_include = re.match(r'^\s*#include\s+"([^"]+)"\s*$', line)
        
        if local_include:
            include_file = local_include.group(1)
            
            # Skip the public header - it will be separate
            if include_file == 'ristretto.h':
                result_lines.append(line)
                continue
                
            # Look for the file in include/ directory first, then current directory
            include_path = None
            possible_paths = [
                os.path.join(base_dir, 'include', include_file),
                os.path.join(base_dir, include_file),
            ]
            
            for path in possible_paths:
                if os.path.exists(path):
                    include_path = path
                    break
                    
            if include_path and include_path not in processed_files:
                processed_files.add(include_path)
                
                # Read and process the included file
                include_content = read_file(include_path)
                if include_content:
                    result_lines.append(f"/* BEGIN {include_file} */")
                    
                    # Process the included file's content recursively
                    processed_include = process_includes(include_content, processed_files, base_dir)
                    result_lines.append(processed_include)
                    
                    result_lines.append(f"/* END {include_file} */")
                else:
                    result_lines.append(f"/* ERROR: Could not read {include_file} */")
                    result_lines.append(line)
            else:
                # Keep the include directive (external header or already processed)
                result_lines.append(line)
        else:
            # Not an include directive, keep as is
            result_lines.append(line)
    
    return '\n'.join(result_lines)

def create_embedded():
    """Create the RistrettoDB embedded"""
    
    # Get the project root directory
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    
    print(f"Project root: {project_root}")
    
    # Define source files to include. V2-only: the engine, the exported
    # ristretto_* wrappers, and version info. (The Original SQL engine and its
    # dead utilities were removed in the V2-only pivot.)
    source_files = [
        'src/version.c',      # Version info first
        'src/table_v2.c',     # Table V2 engine
        'src/ristretto_api.c',# Exported ristretto_* Table V2 wrappers
    ]
    
    # Track processed files to avoid duplicates
    processed_files = set()
    
    # Start building the embedded
    embedded = []

    # Feature-test macro: must precede every #include. Under a strict
    # -std=c11 on glibc (Linux), POSIX/BSD functions used by the storage
    # engine (ftruncate, fsync, flock, mmap/msync) are hidden unless a
    # feature-test macro is defined. _DEFAULT_SOURCE exposes them and is a
    # harmless no-op on macOS/BSD. This keeps `clang -std=c11 ristretto.c`
    # compiling standalone for embedders regardless of their own flags.
    embedded.append("""#if !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif
""")

    # Header comment
    embedded.append("""/*
** RistrettoDB Embedded
**
** This file contains the complete implementation of RistrettoDB in a single
** source file. Simply compile this file along with your application.
**
** To use embedded (Option 1 - Self-contained):
**   1. Include this ristretto.c file in your project
**   2. Define RISTRETTO_EMBEDDED before including
**   3. Example:
**      #define RISTRETTO_EMBEDDED
**      #include "ristretto.c"  // Note: .c not .h
**
** To use with separate header (Option 2):
**   1. Include ristretto.h in your source files
**   2. Compile ristretto.c separately
**   3. Link together
**
** Generated by embed.py
** RistrettoDB Version: 0.3.0
** Homepage: https://github.com/MonkeyIsNull/RistrettoDB
*/

#ifndef RISTRETTO_EMBEDDED

/* When used as a separate compilation unit, include the public header */
#include "ristretto.h"

#else

/* 
** When used as an embedded, provide all definitions inline
** This section replaces ristretto.h when RISTRETTO_EMBEDDED is defined
*/

#ifndef RISTRETTO_H
#define RISTRETTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
** RistrettoDB Version Information
*/
#define RISTRETTO_VERSION        "0.3.0"
#define RISTRETTO_VERSION_NUMBER 3000
#define RISTRETTO_VERSION_MAJOR  0
#define RISTRETTO_VERSION_MINOR  3
#define RISTRETTO_VERSION_PATCH  0

const char* ristretto_version(void);
int ristretto_version_number(void);

/*
** Table V2 API Constants (must match the internal table_v2.h values)
*/
#define RISTRETTO_MAX_COLUMNS 14
#define RISTRETTO_MAX_COLUMN_NAME 32
#define RISTRETTO_TABLE_HEADER_SIZE 1024
#define RISTRETTO_INITIAL_FILE_SIZE (1024 * 1024)
#define RISTRETTO_GROWTH_FACTOR 2
#define RISTRETTO_SYNC_INTERVAL_ROWS 512
#define RISTRETTO_SYNC_INTERVAL_MS 100
#define RISTRETTO_TABLE_MAGIC "RSTRDB\\x00\\x00"
#define RISTRETTO_TABLE_VERSION 3

typedef enum {
    RISTRETTO_COL_INTEGER = 1,
    RISTRETTO_COL_REAL = 2,
    RISTRETTO_COL_TEXT = 3,
    RISTRETTO_COL_NULLABLE = 4
} RistrettoColumnType;

typedef enum {
    RISTRETTO_CREATE_NEW = 0,
    RISTRETTO_CREATE_OR_TRUNCATE = 1,
    RISTRETTO_OPEN_OR_CREATE = 2
} RistrettoOpenMode;

typedef struct RistrettoTable RistrettoTable;

typedef struct {
    RistrettoColumnType type;
    union {
        int64_t integer;
        double real;
        struct {
            char *data;
            size_t length;
        } text;
    } value;
    bool is_null;
} RistrettoValue;

typedef struct {
    char name[RISTRETTO_MAX_COLUMN_NAME];
    uint8_t type;
    uint8_t length;
    uint16_t offset;
    uint8_t reserved[4];
} RistrettoColumnDesc;

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t row_size;
    uint64_t num_rows;
    uint32_t column_count;
    uint8_t reserved[12];
    RistrettoColumnDesc columns[RISTRETTO_MAX_COLUMNS];
} RistrettoTableHeader;

_Static_assert(RISTRETTO_TABLE_HEADER_SIZE >= sizeof(RistrettoTableHeader),
               "row data must not overlap the table header");

/*
** Table V2 API Functions
*/
RistrettoTable* ristretto_table_create(const char *name, const char *schema_sql);
RistrettoTable* ristretto_table_open(const char *name);
RistrettoTable* ristretto_table_create_ex(const char *name, const char *schema_sql,
                                          const char *base_dir, int open_mode);
RistrettoTable* ristretto_table_open_ex(const char *name, const char *base_dir);
void ristretto_table_close(RistrettoTable *table);

bool ristretto_table_append_row(RistrettoTable *table, const RistrettoValue *values);
bool ristretto_table_append_row_n(RistrettoTable *table, const RistrettoValue *values,
                                  uint32_t value_count);
bool ristretto_table_select(RistrettoTable *table,
                           void (*callback)(void *ctx, const RistrettoValue *row), void *ctx);

bool ristretto_table_flush(RistrettoTable *table);
bool ristretto_table_flush_durable(RistrettoTable *table);
bool ristretto_table_remap(RistrettoTable *table);
bool ristretto_table_ensure_space(RistrettoTable *table, size_t needed_bytes);

bool ristretto_table_parse_schema(const char *schema_sql, RistrettoColumnDesc *columns,
                                 uint32_t *column_count, uint32_t *row_size);
const RistrettoColumnDesc* ristretto_table_get_column(RistrettoTable *table, const char *name);
size_t ristretto_table_get_row_count(RistrettoTable *table);

RistrettoValue ristretto_value_integer(int64_t val);
RistrettoValue ristretto_value_real(double val);
RistrettoValue ristretto_value_text(const char *str);
RistrettoValue ristretto_value_null(void);
void ristretto_value_destroy(RistrettoValue *value);

bool ristretto_table_pack_row(RistrettoTable *table, const RistrettoValue *values, uint8_t *row_buffer);
bool ristretto_table_unpack_row(RistrettoTable *table, const uint8_t *row_buffer, RistrettoValue *values);

uint64_t ristretto_get_time_ms(void);
bool ristretto_create_data_directory(void);

/*
** NOTE: The amalgamation deliberately omits the user-facing compatibility
** macros (#define table_create ristretto_table_create, #define Value
** RistrettoValue, ...). In a single translation unit they would rewrite the
** inlined internal engine bodies (which define Value / table_create / ...) and
** collide. Embedding code should call the ristretto_*-prefixed API directly.
*/

#ifdef __cplusplus
}
#endif

#endif /* RISTRETTO_H */

#endif /* !RISTRETTO_EMBEDDED */

/* Standard library includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

""")
    
    # Process each source file
    for source_file in source_files:
        source_path = project_root / source_file
        
        if not source_path.exists():
            print(f"Warning: {source_file} not found, skipping")
            continue
            
        print(f"Processing: {source_file}")
        
        content = read_file(source_path)
        if not content:
            continue
            
        # Add file separator
        embedded.append(f"\n/* BEGIN {source_file} */\n")
        
        # Process includes in this file
        processed_content = process_includes(content, processed_files, str(project_root))
        
        # Remove #include directives for system headers and local headers (already included)
        lines = processed_content.split('\n')
        filtered_lines = []
        
        for line in lines:
            # Skip system includes, ristretto.h include, and local header includes
            if (re.match(r'^\s*#include\s*<', line) or 
                re.match(r'^\s*#include\s*"ristretto\.h"', line) or
                re.match(r'^\s*#include\s*"[^"]+\.h"', line)):
                continue
            filtered_lines.append(line)
        
        embedded.append('\n'.join(filtered_lines))
        embedded.append(f"\n/* END {source_file} */\n")
    
    # Combine everything
    final_content = ''.join(embedded)
    
    # Write the embedded file (the distributed amalgamation lives in embed/)
    output_path = project_root / 'embed' / 'ristretto.c'
    
    if write_file(output_path, final_content):
        print(f"\nEmbedded created successfully!")
        print(f"Output file: {output_path}")
        print(f"Size: {len(final_content):,} characters")
        
        # Show basic statistics
        lines = final_content.split('\n')
        code_lines = [line for line in lines if line.strip() and not line.strip().startswith('//') and not line.strip().startswith('/*')]
        
        print(f"Total lines: {len(lines):,}")
        print(f"Code lines: {len(code_lines):,}")
        print()
        print("To use the embedded:")
        print("  1. Copy ristretto.h and ristretto.c to your project")
        print("  2. Include ristretto.h in your source files")
        print("  3. Compile ristretto.c with your project")
        print("  4. Example: gcc -O3 -o myapp myapp.c ristretto.c")
        
        return True
    
    return False

def main():
    """Main function"""
    print("RistrettoDB Embedded Generator")
    print("==================================")
    
    if create_embedded():
        print("\nSUCCESS: Embedded completed successfully!")
        return 0
    else:
        print("\nERROR: Embedded failed!")
        return 1

if __name__ == '__main__':
    sys.exit(main())