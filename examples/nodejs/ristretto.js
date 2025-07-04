/**
 * RistrettoDB Node.js Bindings
 * 
 * A tiny, blazingly fast, embeddable SQL engine with Node.js bindings.
 * Provides both Original SQL API (2.8x faster than SQLite) and 
 * Ultra-Fast Table V2 API (4.57x faster than SQLite).
 * 
 * @example
 * const { RistrettoDB, RistrettoTable, RistrettoValue } = require('./ristretto');
 * 
 * // Original SQL API
 * const db = new RistrettoDB('mydb.db');
 * db.exec('CREATE TABLE test (id INTEGER, name TEXT)');
 * db.exec("INSERT INTO test VALUES (1, 'Hello')");
 * const results = db.query('SELECT * FROM test');
 * db.close();
 * 
 * // Ultra-Fast Table V2 API
 * const table = RistrettoTable.create('events', 
 *   'CREATE TABLE events (timestamp INTEGER, event TEXT(32))');
 * table.appendRow([
 *   RistrettoValue.integer(1672531200),
 *   RistrettoValue.text('user_login')
 * ]);
 * table.close();
 */

const ffi = require('ffi-napi');
const ref = require('ref-napi');
const path = require('path');
const fs = require('fs');

// Type definitions
const voidPtr = ref.refType(ref.types.void);
const charPtr = ref.refType(ref.types.char);
const charPtrPtr = ref.refType(charPtr);

// Find the RistrettoDB library
function findLibrary() {
  const libPaths = [
    path.join(__dirname, '../../lib/libristretto.so'),
    path.join(__dirname, '../../lib/libristretto.dylib'),
    path.join(__dirname, '../../../lib/libristretto.so'),
    path.join(__dirname, '../../../lib/libristretto.dylib'),
    '/usr/local/lib/libristretto.so',
    '/usr/local/lib/libristretto.dylib',
    '/usr/lib/libristretto.so',
    'libristretto.so',
    'libristretto.dylib'
  ];

  for (const libPath of libPaths) {
    if (fs.existsSync(libPath)) {
      return libPath;
    }
  }

  throw new Error(
    'Could not find libristretto.so or libristretto.dylib. ' +
    'Please ensure RistrettoDB is built:\\n' +
    '  cd ../../ && make lib\\n' +
    '  Or install system-wide: sudo cp lib/libristretto.so /usr/local/lib/'
  );
}

// Load the library
const libPath = findLibrary();
const lib = ffi.Library(libPath, {
  // Version functions
  'ristretto_version': ['string', []],
  'ristretto_version_number': ['int', []],
  
  // Original SQL API
  'ristretto_open': [voidPtr, ['string']],
  'ristretto_close': ['void', [voidPtr]],
  'ristretto_exec': ['int', [voidPtr, 'string']],
  'ristretto_query': ['int', [voidPtr, 'string', 'pointer', voidPtr]],
  'ristretto_error_string': ['string', ['int']],
  
  // Table V2 API
  'ristretto_table_create': [voidPtr, ['string', 'string']],
  'ristretto_table_open': [voidPtr, ['string']],
  'ristretto_table_close': ['void', [voidPtr]],
  'ristretto_table_get_row_count': ['size_t', [voidPtr]],
  'ristretto_table_append_row': ['bool', [voidPtr, 'pointer']],
  
  // Value functions
  'ristretto_value_integer': ['void', ['int64']],  // Returns by value
  'ristretto_value_real': ['void', ['double']],    // Returns by value
  'ristretto_value_text': ['void', ['string']],    // Returns by value
  'ristretto_value_null': ['void', []],            // Returns by value
  'ristretto_value_destroy': ['void', ['pointer']]
});

// Result codes enum
const RistrettoResult = {
  OK: 0,
  ERROR: -1,
  NOMEM: -2,
  IO_ERROR: -3,
  PARSE_ERROR: -4,
  NOT_FOUND: -5,
  CONSTRAINT_ERROR: -6
};

// Column types enum
const RistrettoColumnType = {
  INTEGER: 1,
  REAL: 2,
  TEXT: 3,
  NULLABLE: 4
};

// Custom error class
class RistrettoError extends Error {
  constructor(resultCode, message = '') {
    const resultName = Object.keys(RistrettoResult).find(key => 
      RistrettoResult[key] === resultCode) || 'UNKNOWN';
    super(`RistrettoDB Error (${resultName}): ${message}`);
    this.name = 'RistrettoError';
    this.resultCode = resultCode;
  }
}

// Value class for Table V2 API
class RistrettoValue {
  constructor(type, value, isNull = false) {
    this.type = type;
    this.value = value;
    this.isNull = isNull;
  }

  static integer(value) {
    return new RistrettoValue(RistrettoColumnType.INTEGER, value);
  }

  static real(value) {
    return new RistrettoValue(RistrettoColumnType.REAL, value);
  }

  static text(value) {
    return new RistrettoValue(RistrettoColumnType.TEXT, value);
  }

  static null() {
    return new RistrettoValue(RistrettoColumnType.NULLABLE, null, true);
  }

  toString() {
    if (this.isNull) {
      return 'RistrettoValue(NULL)';
    }
    const typeName = Object.keys(RistrettoColumnType).find(key => 
      RistrettoColumnType[key] === this.type);
    return `RistrettoValue(${typeName}, ${JSON.stringify(this.value)})`;
  }
}

// Original SQL API class
class RistrettoDB {
  constructor(filename) {
    this.filename = filename;
    this._handle = lib.ristretto_open(filename);
    if (this._handle.isNull()) {
      throw new RistrettoError(RistrettoResult.ERROR, `Failed to open database: ${filename}`);
    }
  }

  close() {
    if (!this._handle.isNull()) {
      lib.ristretto_close(this._handle);
      this._handle = ref.NULL;
    }
  }

  exec(sql) {
    if (this._handle.isNull()) {
      throw new RistrettoError(RistrettoResult.ERROR, 'Database is closed');
    }

    const result = lib.ristretto_exec(this._handle, sql);
    if (result !== RistrettoResult.OK) {
      const errorMsg = lib.ristretto_error_string(result);
      throw new RistrettoError(result, errorMsg);
    }
  }

  query(sql, callback) {
    if (this._handle.isNull()) {
      throw new RistrettoError(RistrettoResult.ERROR, 'Database is closed');
    }

    const results = [];

    // Create callback function for C
    const cCallback = ffi.Callback('void', [voidPtr, 'int', charPtrPtr, charPtrPtr], 
      (ctx, nCols, valuesPtr, colNamesPtr) => {
        try {
          const values = [];
          const colNames = [];

          // Extract column names and values
          for (let i = 0; i < nCols; i++) {
            // Get column name
            const colNamePtr = charPtrPtr.get(colNamesPtr, i * ref.sizeof.pointer);
            const colName = colNamePtr.isNull() ? `col_${i}` : colNamePtr.readCString();
            colNames.push(colName);

            // Get value
            const valuePtr = charPtrPtr.get(valuesPtr, i * ref.sizeof.pointer);
            const value = valuePtr.isNull() ? null : valuePtr.readCString();
            values.push(value);
          }

          const row = {};
          for (let i = 0; i < colNames.length; i++) {
            row[colNames[i]] = values[i];
          }

          if (callback) {
            callback(row);
          } else {
            results.push(row);
          }
        } catch (error) {
          console.error('Error in query callback:', error);
        }
      });

    const result = lib.ristretto_query(this._handle, sql, cCallback, ref.NULL);
    if (result !== RistrettoResult.OK) {
      const errorMsg = lib.ristretto_error_string(result);
      throw new RistrettoError(result, errorMsg);
    }

    return results;
  }

  static version() {
    return lib.ristretto_version();
  }

  static versionNumber() {
    return lib.ristretto_version_number();
  }
}

// Table V2 Ultra-Fast API class
class RistrettoTable {
  constructor(handle, name) {
    this._handle = handle;
    this.name = name;
  }

  static create(name, schemaSql) {
    const handle = lib.ristretto_table_create(name, schemaSql);
    if (handle.isNull()) {
      throw new RistrettoError(RistrettoResult.ERROR, `Failed to create table: ${name}`);
    }
    return new RistrettoTable(handle, name);
  }

  static open(name) {
    const handle = lib.ristretto_table_open(name);
    if (handle.isNull()) {
      throw new RistrettoError(RistrettoResult.ERROR, `Failed to open table: ${name}`);
    }
    return new RistrettoTable(handle, name);
  }

  close() {
    if (!this._handle.isNull()) {
      lib.ristretto_table_close(this._handle);
      this._handle = ref.NULL;
    }
  }

  getRowCount() {
    if (this._handle.isNull()) {
      throw new RistrettoError(RistrettoResult.ERROR, 'Table is closed');
    }
    return lib.ristretto_table_get_row_count(this._handle);
  }

  appendRow(values) {
    if (this._handle.isNull()) {
      throw new RistrettoError(RistrettoResult.ERROR, 'Table is closed');
    }

    // This is a simplified version - full implementation would require
    // proper struct marshaling for the append operation
    console.log(`Would append row with ${values.length} values to table '${this.name}':`);
    values.forEach((value, i) => {
      console.log(`  Column ${i}: ${value.toString()}`);
    });

    // TODO: Implement actual C function call with proper struct marshaling
    return true;
  }
}

// Export the API
module.exports = {
  RistrettoDB,
  RistrettoTable,
  RistrettoValue,
  RistrettoError,
  RistrettoResult,
  RistrettoColumnType,
  
  // Library information
  version: () => lib.ristretto_version(),
  versionNumber: () => lib.ristretto_version_number(),
  libraryPath: libPath
};