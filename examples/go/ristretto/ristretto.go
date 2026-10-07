// Package ristretto provides cgo bindings for RistrettoDB.
//
// RistrettoDB is a tiny, embeddable database written in C. It exposes two
// distinct APIs, both surfaced here:
//
//   - The Original SQL API (DB / Open / Exec / Query): a small SQL engine
//     (CREATE TABLE / INSERT / SELECT with a limited expression set). The C
//     API has no parameter binding, so statements are plain SQL strings; use
//     QuoteString for any user-supplied TEXT literal.
//
//   - The Table V2 API (Table / CreateTable / OpenTable / AppendRow / Scan):
//     a fixed-width, append-only, mmap-backed table store. This is the fast
//     write/scan path and is the most complete part of these bindings.
//
// # Building
//
// These bindings link against the static library built from the project's
// C sources. From the repository root:
//
//	make static        # produces lib/libristretto.a
//
// then, from this module (examples/go):
//
//	go build ./...
//	go test ./ristretto
//
// The cgo directives below locate the header in ../../../include and the
// library in ../../../lib relative to this source file.
//
// # Table V2 storage layout
//
// A V2 table named "foo" is stored at "data/foo.rdb" relative to the current
// working directory. CreateTable truncates (O_TRUNC) any existing file;
// OpenTable resumes an existing file and its rows survive across process
// restarts. Rows are fixed width: INTEGER and REAL are 8 bytes, TEXT(n) is n
// bytes (values are truncated to n-1 bytes + NUL).
//
// The on-disk format is version 2 (1024-byte header). Version-1 .rdb files
// written by older builds are not readable and are rejected cleanly on open.
// CreateTable recreates its file each run, so this binding is unaffected.
//
// # Known limitations
//
//   - V2 does not persist NULL-ness: a NULL written to a column reads back as
//     the zero value for that column (0, 0.0, or ""). See NullValue.
//   - The V2 table_select in C ignores the WHERE clause, so Scan returns every
//     row; filter in Go. (The Original SQL API's WHERE is evaluated in C.)
//   - No WAL: Close performs a durable msync+fsync, but a crash mid-run may
//     lose rows written since the last sync.
//   - The SQL API builds queries from strings (no bound parameters).
package ristretto

/*
#cgo CFLAGS: -I${SRCDIR}/../../../include
// Link the static archive directly (by path) so the resulting binary has no
// runtime dependency on libristretto.so. Build it first with: make static
#cgo LDFLAGS: ${SRCDIR}/../../../lib/libristretto.a

#include <stdlib.h>
#include <stdint.h>
#include "cbridge.h"
*/
import "C"

import (
	"fmt"
	"runtime"
	"runtime/cgo"
	"strings"
	"sync"
	"unsafe"
)

// Result mirrors the C RistrettoResult enum.
type Result int

const (
	OK              Result = 0
	Error           Result = -1
	NoMem           Result = -2
	IOError         Result = -3
	ParseError      Result = -4
	NotFound        Result = -5
	ConstraintError Result = -6
)

func (r Result) String() string {
	switch r {
	case OK:
		return "OK"
	case Error:
		return "ERROR"
	case NoMem:
		return "NOMEM"
	case IOError:
		return "IO_ERROR"
	case ParseError:
		return "PARSE_ERROR"
	case NotFound:
		return "NOT_FOUND"
	case ConstraintError:
		return "CONSTRAINT_ERROR"
	default:
		return fmt.Sprintf("UNKNOWN(%d)", int(r))
	}
}

// RistrettoError is returned by operations that fail inside the C library.
type RistrettoError struct {
	Code    Result
	Message string
}

func (e *RistrettoError) Error() string {
	if e.Message != "" {
		return fmt.Sprintf("RistrettoDB error (%s): %s", e.Code.String(), e.Message)
	}
	return fmt.Sprintf("RistrettoDB error (%s)", e.Code.String())
}

// ColumnType identifies a Table V2 column's storage type.
type ColumnType int

const (
	INTEGER ColumnType = 1 // int64, 8 bytes
	REAL    ColumnType = 2 // float64, 8 bytes
	TEXT    ColumnType = 3 // fixed-width TEXT(n)
	NULL    ColumnType = 4 // nullable sentinel
)

func (ct ColumnType) String() string {
	switch ct {
	case INTEGER:
		return "INTEGER"
	case REAL:
		return "REAL"
	case TEXT:
		return "TEXT"
	case NULL:
		return "NULL"
	default:
		return fmt.Sprintf("UNKNOWN(%d)", int(ct))
	}
}

// Value is a single typed cell used for appending to and scanning from a
// Table V2 table.
type Value struct {
	Type   ColumnType
	Data   interface{} // int64, float64, or string depending on Type
	IsNull bool
}

// IntegerValue builds an INTEGER value.
func IntegerValue(v int64) Value { return Value{Type: INTEGER, Data: v} }

// RealValue builds a REAL value.
func RealValue(v float64) Value { return Value{Type: REAL, Data: v} }

// TextValue builds a TEXT value. It is truncated to the column width (n-1
// bytes) on append.
func TextValue(v string) Value { return Value{Type: TEXT, Data: v} }

// NullValue builds a NULL value.
//
// Note: the V2 storage format does not record NULL-ness, so a value written
// with NullValue reads back as the column's zero value (0, 0.0, or "").
func NullValue() Value { return Value{Type: NULL, IsNull: true} }

// Int returns the value as an int64 (0 if it is not an integer).
func (v Value) Int() int64 {
	if i, ok := v.Data.(int64); ok {
		return i
	}
	return 0
}

// Float returns the value as a float64 (0 if it is not a real).
func (v Value) Float() float64 {
	if f, ok := v.Data.(float64); ok {
		return f
	}
	return 0
}

// Text returns the value as a string ("" if it is not text).
func (v Value) Text() string {
	if s, ok := v.Data.(string); ok {
		return s
	}
	return ""
}

func (v Value) String() string {
	if v.IsNull {
		return "NULL"
	}
	return fmt.Sprintf("%v", v.Data)
}

// Version returns the RistrettoDB version string, e.g. "2.0.0".
func Version() string { return C.GoString(C.ristretto_version()) }

// VersionNumber returns the packed numeric version.
func VersionNumber() int { return int(C.ristretto_version_number()) }

// QuoteString wraps a Go string as a single-quoted SQL TEXT literal for the
// Original SQL API (which has no bound parameters). Embedded single quotes are
// doubled per the SQL standard, but note that the current C parser does not
// accept escaped quotes, so strings containing a single quote cannot be used
// as TEXT literals with this engine.
func QuoteString(s string) string {
	return "'" + strings.ReplaceAll(s, "'", "''") + "'"
}

// =============================================================================
// Original SQL API
// =============================================================================

// DB is a handle to the Original SQL API database.
type DB struct {
	handle *C.RistrettoDB
	mu     sync.Mutex
	closed bool
}

// Open opens or creates a RistrettoDB SQL database file.
func Open(filename string) (*DB, error) {
	cName := C.CString(filename)
	defer C.free(unsafe.Pointer(cName))

	h := C.ristretto_open(cName)
	if h == nil {
		return nil, &RistrettoError{Code: Error, Message: "failed to open database: " + filename}
	}
	db := &DB{handle: h}
	runtime.SetFinalizer(db, (*DB).Close)
	return db, nil
}

// Close releases the database. It is safe to call more than once.
func (db *DB) Close() error {
	db.mu.Lock()
	defer db.mu.Unlock()
	if !db.closed && db.handle != nil {
		C.ristretto_close(db.handle)
		db.handle = nil
		db.closed = true
		runtime.SetFinalizer(db, nil)
	}
	return nil
}

// Exec runs a DDL/DML statement (CREATE TABLE, INSERT, ...).
func (db *DB) Exec(sql string) error {
	db.mu.Lock()
	defer db.mu.Unlock()
	if db.closed {
		return &RistrettoError{Code: Error, Message: "database is closed"}
	}
	cSQL := C.CString(sql)
	defer C.free(unsafe.Pointer(cSQL))

	res := Result(C.ristretto_exec(db.handle, cSQL))
	if res != OK {
		return &RistrettoError{Code: res, Message: C.GoString(C.ristretto_error_string(C.int(res)))}
	}
	return nil
}

// QueryResult is a single result row keyed by column name.
type QueryResult map[string]string

type queryCollector struct {
	rows []QueryResult
}

// Query runs a SELECT and returns the result rows.
func (db *DB) Query(sql string) ([]QueryResult, error) {
	db.mu.Lock()
	defer db.mu.Unlock()
	if db.closed {
		return nil, &RistrettoError{Code: Error, Message: "database is closed"}
	}
	cSQL := C.CString(sql)
	defer C.free(unsafe.Pointer(cSQL))

	col := &queryCollector{}
	h := cgo.NewHandle(col)
	defer h.Delete()

	res := Result(C.rdb_query(db.handle, cSQL, unsafe.Pointer(&h)))
	if res != OK {
		return nil, &RistrettoError{Code: res, Message: C.GoString(C.ristretto_error_string(C.int(res)))}
	}
	return col.rows, nil
}

// =============================================================================
// Table V2 API
// =============================================================================

// ColumnInfo describes one column of a Table V2 table.
type ColumnInfo struct {
	Name string
	Type ColumnType
}

// Table is a handle to a Table V2 append-only table.
type Table struct {
	handle  *C.Table
	name    string
	columns []ColumnInfo
	mu      sync.Mutex
	closed  bool
}

// CreateTable creates (truncating any existing file) a V2 table named name
// using the given "CREATE TABLE ..." schema. The file lives at
// data/<name>.rdb relative to the current working directory.
func CreateTable(name, schemaSQL string) (*Table, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	cSchema := C.CString(schemaSQL)
	defer C.free(unsafe.Pointer(cSchema))

	h := C.table_create(cName, cSchema)
	if h == nil {
		return nil, &RistrettoError{Code: Error, Message: "failed to create table: " + name}
	}
	return newTable(h, name), nil
}

// OpenTable opens an existing V2 table named name, resuming its rows.
func OpenTable(name string) (*Table, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	h := C.table_open(cName)
	if h == nil {
		return nil, &RistrettoError{Code: Error, Message: "failed to open table: " + name}
	}
	return newTable(h, name), nil
}

func newTable(h *C.Table, name string) *Table {
	t := &Table{handle: h, name: name}
	n := int(C.rdb_col_count(h))
	t.columns = make([]ColumnInfo, n)
	for i := 0; i < n; i++ {
		t.columns[i] = ColumnInfo{
			Name: C.GoString(C.rdb_col_name(h, C.uint(i))),
			Type: ColumnType(C.rdb_col_type(h, C.uint(i))),
		}
	}
	runtime.SetFinalizer(t, (*Table).Close)
	return t
}

// Columns returns the table's schema in column order.
func (t *Table) Columns() []ColumnInfo {
	out := make([]ColumnInfo, len(t.columns))
	copy(out, t.columns)
	return out
}

// Name returns the table name.
func (t *Table) Name() string { return t.name }

// Close flushes and releases the table. Safe to call more than once.
func (t *Table) Close() error {
	t.mu.Lock()
	defer t.mu.Unlock()
	if !t.closed && t.handle != nil {
		C.table_close(t.handle)
		t.handle = nil
		t.closed = true
		runtime.SetFinalizer(t, nil)
	}
	return nil
}

// RowCount returns the number of rows currently stored.
func (t *Table) RowCount() int64 {
	t.mu.Lock()
	defer t.mu.Unlock()
	if t.closed {
		return 0
	}
	return int64(C.table_get_row_count(t.handle))
}

// AppendRow appends one row. values must contain exactly one Value per column
// in column order, and each Value's type must match its column's type.
func (t *Table) AppendRow(values []Value) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	if t.closed {
		return &RistrettoError{Code: Error, Message: "table is closed"}
	}
	if len(values) != len(t.columns) {
		return &RistrettoError{Code: Error, Message: fmt.Sprintf(
			"expected %d values, got %d", len(t.columns), len(values))}
	}

	cvals := make([]C.Value, len(values))
	// Track C strings allocated for TEXT so we can free them after the call.
	for i, v := range values {
		switch {
		case v.IsNull || v.Type == NULL:
			cvals[i] = C.value_null()
		case v.Type == INTEGER:
			cvals[i] = C.value_integer(C.int64_t(v.Int()))
		case v.Type == REAL:
			cvals[i] = C.value_real(C.double(v.Float()))
		case v.Type == TEXT:
			cs := C.CString(v.Text())
			cvals[i] = C.value_text(cs) // value_text copies the string
			C.free(unsafe.Pointer(cs))
		default:
			return &RistrettoError{Code: Error, Message: "unsupported value type: " + v.Type.String()}
		}
	}
	// value_text allocates its own buffer; free all of them afterwards.
	defer func() {
		for i := range cvals {
			C.value_destroy(&cvals[i])
		}
	}()

	if !bool(C.table_append_row(t.handle, &cvals[0])) {
		return &RistrettoError{Code: Error, Message: "append_row failed"}
	}
	return nil
}

// Row is a single row scanned from a table, in column order.
type Row struct {
	Columns []ColumnInfo
	Values  []Value
}

// Get returns the value for the named column, or an empty Value if absent.
func (r Row) Get(name string) Value {
	for i, c := range r.Columns {
		if c.Name == name {
			return r.Values[i]
		}
	}
	return Value{}
}

// scanState is passed through cgo via a cgo.Handle during a scan.
type scanState struct {
	table *Table
	fn    func(Row) bool
	stop  bool
	err   error
}

// ForEach scans every row (WHERE is not yet implemented in C, so all rows are
// visited) and calls fn for each. Return false from fn to stop early.
func (t *Table) ForEach(fn func(Row) bool) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	if t.closed {
		return &RistrettoError{Code: Error, Message: "table is closed"}
	}
	st := &scanState{table: t, fn: fn}
	h := cgo.NewHandle(st)
	defer h.Delete()

	if C.rdb_table_select(t.handle, unsafe.Pointer(&h)) != 0 {
		return &RistrettoError{Code: Error, Message: "select failed"}
	}
	return st.err
}

// Scan returns all rows in the table.
func (t *Table) Scan() ([]Row, error) {
	var rows []Row
	err := t.ForEach(func(r Row) bool {
		rows = append(rows, r)
		return true
	})
	return rows, err
}
