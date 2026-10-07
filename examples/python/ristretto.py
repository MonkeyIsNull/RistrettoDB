"""
RistrettoDB Python Bindings

A tiny, blazingly fast, embeddable SQL engine with Python bindings.
Provides both Original SQL API (2.8x faster than SQLite) and 
Ultra-Fast Table V2 API (4.57x faster than SQLite).

Usage:
    from ristretto import RistrettoDB, RistrettoTable, RistrettoValue
    
    # Original SQL API
    db = RistrettoDB("mydb.db")
    db.exec("CREATE TABLE test (id INTEGER, name TEXT)")
    db.exec("INSERT INTO test VALUES (1, 'Hello')")
    results = db.query("SELECT * FROM test")
    db.close()
    
    # Ultra-Fast Table V2 API
    table = RistrettoTable.create("events", 
        "CREATE TABLE events (timestamp INTEGER, event TEXT(32))")
    table.append_row([RistrettoValue.integer(1672531200), 
                      RistrettoValue.text("user_login")])
    table.close()
"""

import ctypes
import os
import sys
from typing import List, Optional, Callable, Any, Union
from enum import IntEnum
from dataclasses import dataclass

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
        candidates.append(os.path.join(here, n))       # alongside this file
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

# Function signatures
_lib.ristretto_version.restype = ctypes.c_char_p
_lib.ristretto_version_number.restype = ctypes.c_int

_lib.ristretto_open.argtypes = [ctypes.c_char_p]
_lib.ristretto_open.restype = ctypes.c_void_p

_lib.ristretto_close.argtypes = [ctypes.c_void_p]
_lib.ristretto_close.restype = None

_lib.ristretto_exec.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
_lib.ristretto_exec.restype = ctypes.c_int

_lib.ristretto_error_string.argtypes = [ctypes.c_int]
_lib.ristretto_error_string.restype = ctypes.c_char_p

# Table V2 API signatures
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

_QUERY_CB_TYPE = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_int,
                                  ctypes.POINTER(ctypes.c_char_p),
                                  ctypes.POINTER(ctypes.c_char_p))
_lib.ristretto_query.argtypes = [
    ctypes.c_void_p, ctypes.c_char_p, _QUERY_CB_TYPE, ctypes.c_void_p]
_lib.ristretto_query.restype = ctypes.c_int

# Value structure
class RistrettoValue:
    """Represents a value in RistrettoDB Table V2 API"""
    
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
        """Create a null value"""
        return cls(RistrettoColumnType.NULLABLE, None, True)
    
    def __repr__(self):
        if self.is_null:
            return "RistrettoValue(NULL)"
        return f"RistrettoValue({self.type.name}, {self.value!r})"

class RistrettoDB:
    """
    RistrettoDB Original SQL API
    
    Provides 2.8x faster performance than SQLite for general SQL operations.
    Supports CREATE TABLE, INSERT, SELECT with WHERE clauses.
    """
    
    def __init__(self, filename: str):
        """Open or create a RistrettoDB database"""
        self.filename = filename
        self._handle = _lib.ristretto_open(filename.encode('utf-8'))
        if not self._handle:
            raise RistrettoError(RistrettoResult.ERROR, f"Failed to open database: {filename}")
    
    def close(self):
        """Close the database"""
        if self._handle:
            _lib.ristretto_close(self._handle)
            self._handle = None
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
    
    def exec(self, sql: str) -> None:
        """Execute a SQL statement (DDL/DML)"""
        if not self._handle:
            raise RistrettoError(RistrettoResult.ERROR, "Database is closed")
        
        result = _lib.ristretto_exec(self._handle, sql.encode('utf-8'))
        if result != RistrettoResult.OK:
            error_msg = _lib.ristretto_error_string(result).decode('utf-8')
            raise RistrettoError(RistrettoResult(result), error_msg)
    
    def query(self, sql: str, callback: Optional[Callable] = None) -> List[dict]:
        """
        Execute a SQL query and return results
        
        If callback is provided, it will be called for each row.
        Otherwise, returns a list of dictionaries.
        """
        if not self._handle:
            raise RistrettoError(RistrettoResult.ERROR, "Database is closed")
        
        results = []
        
        def internal_callback(ctx, n_cols, values_ptr, col_names_ptr):
            # Convert C arrays to Python
            values = []
            col_names = []
            
            for i in range(n_cols):
                # Get column name
                col_name_ptr = ctypes.cast(col_names_ptr, ctypes.POINTER(ctypes.c_char_p))[i]
                col_name = col_name_ptr.decode('utf-8') if col_name_ptr else f"col_{i}"
                col_names.append(col_name)
                
                # Get value
                value_ptr = ctypes.cast(values_ptr, ctypes.POINTER(ctypes.c_char_p))[i]
                value = value_ptr.decode('utf-8') if value_ptr else None
                values.append(value)
            
            row = dict(zip(col_names, values))
            
            if callback:
                callback(row)
            else:
                results.append(row)
        
        # Convert Python callback to C callback
        c_callback = _QUERY_CB_TYPE(internal_callback)
        
        # Execute query
        result = _lib.ristretto_query(self._handle, sql.encode('utf-8'), c_callback, None)
        if result != RistrettoResult.OK:
            error_msg = _lib.ristretto_error_string(result).decode('utf-8')
            raise RistrettoError(RistrettoResult(result), error_msg)
        
        return results
    
    @staticmethod
    def version() -> str:
        """Get RistrettoDB version string"""
        return _lib.ristretto_version().decode('utf-8')
    
    @staticmethod
    def version_number() -> int:
        """Get RistrettoDB version number"""
        return _lib.ristretto_version_number()

class RistrettoTable:
    """
    RistrettoDB Table V2 Ultra-Fast API
    
    Provides 4.57x faster performance than SQLite for append-only workloads.
    Optimized for high-speed logging, telemetry, and analytics ingestion.
    """
    
    def __init__(self, handle: ctypes.c_void_p, name: str):
        """Initialize with existing handle (use create() or open() class methods)"""
        self._handle = handle
        self.name = name
    
    @classmethod
    def create(cls, name: str, schema_sql: str) -> 'RistrettoTable':
        """Create a new ultra-fast table"""
        handle = _lib.ristretto_table_create(name.encode('utf-8'), schema_sql.encode('utf-8'))
        if not handle:
            raise RistrettoError(RistrettoResult.ERROR, f"Failed to create table: {name}")
        return cls(handle, name)
    
    @classmethod
    def open(cls, name: str) -> 'RistrettoTable':
        """Open an existing ultra-fast table"""
        handle = _lib.ristretto_table_open(name.encode('utf-8'))
        if not handle:
            raise RistrettoError(RistrettoResult.ERROR, f"Failed to open table: {name}")
        return cls(handle, name)
    
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

def demo():
    """Demonstration of RistrettoDB Python bindings"""
    print("RistrettoDB Python Bindings Demo")
    print("=" * 40)
    print(f"Library Version: {RistrettoDB.version()}")
    print(f"Library Path: {_lib_path}")
    print()
    
    # Demo Original SQL API
    print("1. Original SQL API Demo (2.8x faster than SQLite)")
    print("-" * 50)
    
    try:
        with RistrettoDB("python_demo.db") as db:
            print("SUCCESS: Database opened successfully")
            
            # Create table
            db.exec("CREATE TABLE inventory (id INTEGER, item TEXT, price REAL)")
            print("SUCCESS: Table created")
            
            # Insert data
            items = [
                "INSERT INTO inventory VALUES (1, 'Laptop', 999.99)",
                "INSERT INTO inventory VALUES (2, 'Mouse', 29.99)",
                "INSERT INTO inventory VALUES (3, 'Keyboard', 79.99)"
            ]
            
            for sql in items:
                db.exec(sql)
            print("SUCCESS: Data inserted")
            
            # Query data
            results = db.query("SELECT * FROM inventory")
            print(f"SUCCESS: Query executed, found {len(results)} rows:")
            for row in results:
                print(f"   {row}")
        
        print("SUCCESS: Original SQL API demo completed\n")
        
    except RistrettoError as e:
        print(f"ERROR: SQL API Error: {e}")
    
    # Demo Table V2 API
    print("2. Table V2 Ultra-Fast API Demo (4.57x faster than SQLite)")
    print("-" * 60)
    
    try:
        with RistrettoTable.create("python_v2_demo", 
                                 "CREATE TABLE python_v2_demo (id INTEGER, name TEXT(32), value REAL)") as table:
            print("SUCCESS: Ultra-fast table created")
            
            # Insert high-speed data
            values = [
                RistrettoValue.integer(1),
                RistrettoValue.text("test_record"),
                RistrettoValue.real(123.45)
            ]
            
            success = table.append_row(values)
            if success:
                print("SUCCESS: High-speed row insertion completed")
                print(f"   Total rows: {table.get_row_count()}")
        
        print("SUCCESS: Table V2 ultra-fast demo completed")
        
    except RistrettoError as e:
        print(f"ERROR: Table V2 API Error: {e}")
    
    print("\nSUCCESS: Python bindings demo completed!")
    print("\nIntegration Examples:")
    print("  • IoT sensor data logging")
    print("  • High-frequency trading data")
    print("  • Real-time analytics ingestion")
    print("  • Security audit trails")

if __name__ == "__main__":
    demo()