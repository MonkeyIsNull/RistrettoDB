package ristretto

/*
#include <stdlib.h>
#include "cbridge.h"
*/
import "C"

import (
	"runtime/cgo"
	"unsafe"
)

// goSelectCallback is invoked by the C table_select scan once per row via the
// rdb_table_select trampoline. ctx points to the cgo.Handle wrapping the
// *scanState for this scan; row points to an array of column_count Values that
// is only valid for the duration of this call, so everything is copied out.
//
//export goSelectCallback
func goSelectCallback(ctx unsafe.Pointer, row *C.Value) {
	st := (*cgo.Handle)(ctx).Value().(*scanState)
	if st.stop {
		return
	}

	cols := st.table.columns
	values := make([]Value, len(cols))
	for i := range cols {
		cv := C.rdbv_at(row, C.int(i))
		values[i] = valueFromC(cv, cols[i].Type)
	}

	colsCopy := make([]ColumnInfo, len(cols))
	copy(colsCopy, cols)

	if !st.fn(Row{Columns: colsCopy, Values: values}) {
		st.stop = true
	}
}

// valueFromC converts a single C Value into a Go Value. colType is the
// declared column type, used when the stored value carries no type of its own.
func valueFromC(cv *C.Value, colType ColumnType) Value {
	if C.rdbv_is_null(cv) != 0 {
		return Value{Type: NULL, IsNull: true}
	}
	switch ColumnType(C.rdbv_type(cv)) {
	case INTEGER:
		return Value{Type: INTEGER, Data: int64(C.rdbv_int(cv))}
	case REAL:
		return Value{Type: REAL, Data: float64(C.rdbv_real(cv))}
	case TEXT:
		return Value{Type: TEXT, Data: C.GoString(C.rdbv_text(cv))}
	default:
		// Fall back to the declared column type.
		switch colType {
		case INTEGER:
			return Value{Type: INTEGER, Data: int64(C.rdbv_int(cv))}
		case REAL:
			return Value{Type: REAL, Data: float64(C.rdbv_real(cv))}
		default:
			return Value{Type: TEXT, Data: C.GoString(C.rdbv_text(cv))}
		}
	}
}

// goQueryCallback is invoked by the C ristretto_query engine once per result
// row via the rdb_query trampoline. ctx points to the cgo.Handle wrapping the
// *queryCollector.
//
//export goQueryCallback
func goQueryCallback(ctx unsafe.Pointer, nCols C.int, values **C.char, colNames **C.char) {
	col := (*cgo.Handle)(ctx).Value().(*queryCollector)

	n := int(nCols)
	valSlice := unsafe.Slice(values, n)
	nameSlice := unsafe.Slice(colNames, n)

	row := make(QueryResult, n)
	for i := 0; i < n; i++ {
		var name string
		if nameSlice[i] != nil {
			name = C.GoString(nameSlice[i])
		} else {
			name = "col_" + itoa(i)
		}
		var val string
		if valSlice[i] != nil {
			val = C.GoString(valSlice[i])
		}
		row[name] = val
	}
	col.rows = append(col.rows, row)
}

func itoa(i int) string {
	if i == 0 {
		return "0"
	}
	var buf [20]byte
	pos := len(buf)
	for i > 0 {
		pos--
		buf[pos] = byte('0' + i%10)
		i /= 10
	}
	return string(buf[pos:])
}
