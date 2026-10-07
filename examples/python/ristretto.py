"""
RistrettoDB Python Bindings (Table V2)

RistrettoDB is a fast, embeddable, fixed-schema, append-only, single-writer
telemetry/analytics store written in C. It is NOT a general-purpose SQL
database: there is no JOIN, UPDATE, DELETE, or transactions, and exactly one
writer at a time.

Usage:
    from ristretto import RistrettoTable, RistrettoValue, version

    table = RistrettoTable.create("events",
        "CREATE TABLE events (timestamp INTEGER, event TEXT(32))")
    table.append_row([RistrettoValue.integer(1672531200),
                      RistrettoValue.text("user_login")])
    for row in table.scan():
        print(row)
    table.close()

Build the shared library first (from the repo root):
    make dynamic    # lib/libristretto.so (Linux) / lib/libristretto.dylib (macOS)
"""

import ctypes
import os
from typing import Any, Callable, List, Optional
from enum import IntEnum


# Determine library path
def _find_library():
    """Find the RistrettoDB shared library.

    Resolves relative to this file (not the process CWD), trying the in-repo
    build output first, then the usual system locations. Both the Linux ".so"
    and macOS ".dylib" names are attempted.
    """
    here = os.path.dirname(os.path.abspath(__file__))
    repo_lib = os.path.normpath(os.path.join(here, "..", "..", "lib"))
    names = ["libristretto.so", "libristretto.dylib"]

    candidates = []
    for n in names:
        candidates.append(os.path.join(repo_lib, n))   # <repo>/lib/<name>
        candidates.append(os.path.join(here, n))        # alongside this file
    candidates += [
        "/usr/local/lib/libristretto.so",
        "/usr/local/lib/libristretto.dylib",
        "/usr/lib/libristretto.so",
    ]

    for path in candidates:
        if os.path.exists(path):
            return path

    raise RuntimeError(
        "Could not find libristretto shared library. Build it first:\n"
        "  cd <repo root> && make dynamic\n"
        "  (produces lib/libristretto.so on Linux, lib/libristretto.dylib on macOS)"
    )


# Load the library
_lib_path = _find_library()
_lib = ctypes.CDLL(_lib_path)


class RistrettoResult(IntEnum):
    """Return codes from RistrettoDB functions"""
    OK = 0
    ERROR = -1
    NOMEM = -2
    IO_ERROR = -3
    PARSE_ERROR = -4
    NOT_FOUND = -5
    CONSTRAINT_ERROR = -6


class RistrettoColumnType(IntEnum):
    """Column data types for Table V2 API"""
    INTEGER = 1
    REAL = 2
    TEXT = 3
    NULLABLE = 4


class RistrettoException(Exception):
    """Base exception for RistrettoDB errors"""
    pass


class RistrettoError(RistrettoException):
    """Database operation error"""
    def __init__(self, result_code: RistrettoResult, message: str = ""):
        self.result_code = result_code
        super().__init__(f"RistrettoDB Error ({result_code.name}): {message}")


# ---- Version ---------------------------------------------------------------
_lib.ristretto_version.restype = ctypes.c_char_p
_lib.ristretto_version_number.restype = ctypes.c_int


def version() -> str:
    """Return the RistrettoDB version string, e.g. '0.3.0'."""
    return _lib.ristretto_version().decode("utf-8")


def version_number() -> int:
    """Return the packed numeric version."""
    return _lib.ristretto_version_number()


# ---- Table V2 API signatures -----------------------------------------------
_lib.ristretto_table_create.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
_lib.ristretto_table_create.restype = ctypes.c_void_p

_lib.ristretto_table_open.argtypes = [ctypes.c_char_p]
_lib.ristretto_table_open.restype = ctypes.c_void_p

_lib.ristretto_table_close.argtypes = [ctypes.c_void_p]
_lib.ristretto_table_close.restype = None

_lib.ristretto_table_get_row_count.argtypes = [ctypes.c_void_p]
_lib.ristretto_table_get_row_count.restype = ctypes.c_size_t

# --- Table V2 Value struct (must match the C RistrettoValue layout) ---------
# C layout: { RistrettoColumnType type; union { int64 | double | {char*,size_t} };
#             bool is_null; }  -> 4-byte enum, 4 pad, 16-byte union, 1 bool, 7 pad
# = 32 bytes on LP64 (arm64 / x86-64).
class _CText(ctypes.Structure):
    _fields_ = [("data", ctypes.c_char_p), ("length", ctypes.c_size_t)]


class _CValueUnion(ctypes.Union):
    _fields_ = [("integer", ctypes.c_int64),
                ("real", ctypes.c_double),
                ("text", _CText)]


class _CValue(ctypes.Structure):
    _fields_ = [("type", ctypes.c_int),
                ("value", _CValueUnion),
                ("is_null", ctypes.c_bool)]


assert ctypes.sizeof(_CValue) == 32, \
    f"RistrettoValue ABI mismatch: expected 32 bytes, got {ctypes.sizeof(_CValue)}"

# Value constructors return the struct by value.
_lib.ristretto_value_integer.argtypes = [ctypes.c_int64]
_lib.ristretto_value_integer.restype = _CValue

_lib.ristretto_value_real.argtypes = [ctypes.c_double]
_lib.ristretto_value_real.restype = _CValue

_lib.ristretto_value_text.argtypes = [ctypes.c_char_p]
_lib.ristretto_value_text.restype = _CValue

_lib.ristretto_value_null.argtypes = []
_lib.ristretto_value_null.restype = _CValue

_lib.ristretto_value_destroy.argtypes = [ctypes.POINTER(_CValue)]
_lib.ristretto_value_destroy.restype = None

# Counted append is the recommended entry point (validates the value count).
_lib.ristretto_table_append_row_n.argtypes = [
    ctypes.c_void_p, ctypes.POINTER(_CValue), ctypes.c_uint32]
_lib.ristretto_table_append_row_n.restype = ctypes.c_bool

_lib.ristretto_table_create_ex.argtypes = [
    ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
_lib.ristretto_table_create_ex.restype = ctypes.c_void_p

_lib.ristretto_table_open_ex.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
_lib.ristretto_table_open_ex.restype = ctypes.c_void_p

# V2 scan callback: fired once per row with a pointer to an array of
# column_count RistrettoValue structs (valid only for the duration of the
# call). The C V2 API has no WHERE clause; it scans every row.
_SELECT_CB_TYPE = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.POINTER(_CValue))
_lib.ristretto_table_select.argtypes = [
    ctypes.c_void_p, _SELECT_CB_TYPE, ctypes.c_void_p]
_lib.ristretto_table_select.restype = ctypes.c_bool


def _count_schema_columns(schema_sql: str) -> int:
    """Count columns in a `CREATE TABLE name (...)` schema by counting the
    top-level commas between the outermost parentheses. The C scan callback
    hands back a bare pointer with no count, so select() needs to know how many
    RistrettoValue structs to decode per row.
    """
    open_idx = schema_sql.find("(")
    if open_idx < 0:
        return 0
    depth = 0
    cols = 1
    for ch in schema_sql[open_idx:]:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                break
        elif ch == "," and depth == 1:
            cols += 1
    return cols


# Value structure
class RistrettoValue:
    """Represents a value in the RistrettoDB Table V2 API."""

    def __init__(self, type_: RistrettoColumnType, value: Any, is_null: bool = False):
        self.type = type_
        self.value = value
        self.is_null = is_null

    @classmethod
    def integer(cls, value: int) -> 'RistrettoValue':
        """Create an integer value"""
        return cls(RistrettoColumnType.INTEGER, value)

    @classmethod
    def real(cls, value: float) -> 'RistrettoValue':
        """Create a real (float) value"""
        return cls(RistrettoColumnType.REAL, value)

    @classmethod
    def text(cls, value: str) -> 'RistrettoValue':
        """Create a text value"""
        return cls(RistrettoColumnType.TEXT, value)

    @classmethod
    def null(cls) -> 'RistrettoValue':
        """Create a NULL value (persists and round-trips as NULL)."""
        return cls(RistrettoColumnType.NULLABLE, None, True)

    def __repr__(self):
        if self.is_null:
            return "RistrettoValue(NULL)"
        return f"RistrettoValue({self.type.name}, {self.value!r})"


class RistrettoTable:
    """
    RistrettoDB Table V2 API.

    A fixed-schema, append-only, mmap-backed table store optimised for
    high-speed logging, telemetry, and analytics ingestion.
    """

    def __init__(self, handle: ctypes.c_void_p, name: str, column_count: int = 0):
        """Initialize with an existing handle (use create()/open())."""
        self._handle = handle
        self.name = name
        # 0 when unknown (e.g. opened without a schema); needed by scan().
        self.column_count = column_count

    @classmethod
    def create(cls, name: str, schema_sql: str) -> 'RistrettoTable':
        """Create a new table (truncating any existing file)."""
        handle = _lib.ristretto_table_create(name.encode('utf-8'), schema_sql.encode('utf-8'))
        if not handle:
            raise RistrettoError(RistrettoResult.ERROR, f"Failed to create table: {name}")
        return cls(handle, name, _count_schema_columns(schema_sql))

    @classmethod
    def open(cls, name: str, column_count: int = 0) -> 'RistrettoTable':
        """Open an existing table, resuming its rows. Pass column_count so
        scan() knows how many values to decode per row."""
        handle = _lib.ristretto_table_open(name.encode('utf-8'))
        if not handle:
            raise RistrettoError(RistrettoResult.ERROR, f"Failed to open table: {name}")
        return cls(handle, name, column_count)

    def close(self):
        """Close the table"""
        if self._handle:
            _lib.ristretto_table_close(self._handle)
            self._handle = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def get_row_count(self) -> int:
        """Get the number of rows in the table"""
        if not self._handle:
            raise RistrettoError(RistrettoResult.ERROR, "Table is closed")
        return _lib.ristretto_table_get_row_count(self._handle)

    def append_row(self, values: List[RistrettoValue]) -> bool:
        """
        Append one row to the table.

        Builds a C array of RistrettoValue structs via the exported value
        constructors and calls the counted append (ristretto_table_append_row_n,
        which validates the value count). TEXT values are allocated by the C
        constructor and freed with ristretto_value_destroy after the append
        copies them into the row buffer, so nothing leaks.
        """
        if not self._handle:
            raise RistrettoError(RistrettoResult.ERROR, "Table is closed")

        n = len(values)
        arr = (_CValue * n)()
        for i, v in enumerate(values):
            if v.is_null or v.type == RistrettoColumnType.NULLABLE:
                arr[i] = _lib.ristretto_value_null()
            elif v.type == RistrettoColumnType.INTEGER:
                arr[i] = _lib.ristretto_value_integer(ctypes.c_int64(int(v.value)))
            elif v.type == RistrettoColumnType.REAL:
                arr[i] = _lib.ristretto_value_real(ctypes.c_double(float(v.value)))
            elif v.type == RistrettoColumnType.TEXT:
                arr[i] = _lib.ristretto_value_text(str(v.value).encode('utf-8'))
            else:
                raise RistrettoError(RistrettoResult.ERROR,
                                     f"Unsupported value type: {v.type}")
        try:
            ok = _lib.ristretto_table_append_row_n(self._handle, arr, n)
        finally:
            for i in range(n):
                _lib.ristretto_value_destroy(ctypes.byref(arr[i]))

        if not ok:
            raise RistrettoError(RistrettoResult.ERROR,
                                 f"append_row failed for table '{self.name}'")
        return True

    def select(self, callback: Optional[Callable[[List[Any]], None]] = None,
               column_count: Optional[int] = None) -> List[List[Any]]:
        """
        Scan every row (V2 has no WHERE clause; filter in Python), invoking
        callback(values) once per row with one decoded Python value per column
        (int / float / str / None). A NULL decodes to None, keyed off the C
        is_null flag. Returns the list of per-row value lists.
        """
        if not self._handle:
            raise RistrettoError(RistrettoResult.ERROR, "Table is closed")
        ncols = column_count if column_count is not None else self.column_count
        if not ncols or ncols < 1:
            raise RistrettoError(
                RistrettoResult.ERROR,
                f"column_count for table '{self.name}' is unknown; pass it to select()")

        rows: List[List[Any]] = []

        def _cb(ctx, row_ptr):
            values: List[Any] = []
            for i in range(ncols):
                cv = row_ptr[i]
                if cv.is_null or cv.type == RistrettoColumnType.NULLABLE:
                    values.append(None)
                elif cv.type == RistrettoColumnType.INTEGER:
                    values.append(int(cv.value.integer))
                elif cv.type == RistrettoColumnType.REAL:
                    values.append(float(cv.value.real))
                elif cv.type == RistrettoColumnType.TEXT:
                    data = cv.value.text.data
                    values.append(data.decode('utf-8') if data is not None else "")
                else:
                    values.append(None)
            if callback:
                callback(values)
            else:
                rows.append(values)

        c_cb = _SELECT_CB_TYPE(_cb)
        ok = _lib.ristretto_table_select(self._handle, c_cb, None)
        if not ok:
            raise RistrettoError(RistrettoResult.ERROR,
                                 f"select failed for table '{self.name}'")
        return rows

    # scan() is an alias for select() that always collects and returns rows.
    def scan(self, column_count: Optional[int] = None) -> List[List[Any]]:
        return self.select(callback=None, column_count=column_count)


def demo():
    """Demonstration of the RistrettoDB Python bindings (Table V2)."""
    print("RistrettoDB Python Bindings Demo (Table V2)")
    print("=" * 44)
    print(f"Library Version: {version()}")
    print(f"Library Path: {_lib_path}")
    print()

    with RistrettoTable.create(
            "python_v2_demo",
            "CREATE TABLE python_v2_demo (id INTEGER, name TEXT(32), value REAL)") as table:
        print("SUCCESS: table created")
        table.append_row([RistrettoValue.integer(1),
                          RistrettoValue.text("test_record"),
                          RistrettoValue.real(123.45)])
        # A row with a NULL text field to show NULL round-trips.
        table.append_row([RistrettoValue.integer(2),
                          RistrettoValue.null(),
                          RistrettoValue.real(0.0)])
        print(f"   Total rows: {table.get_row_count()}")
        print("   Scan:")
        for row in table.scan():
            print(f"     {row}")

    print("\nSUCCESS: Python bindings demo completed!")


if __name__ == "__main__":
    demo()
