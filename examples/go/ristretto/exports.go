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
