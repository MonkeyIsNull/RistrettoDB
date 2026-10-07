/**
 * RistrettoDB Node.js Bindings (koffi)
 *
 * A tiny, embeddable SQL engine. Exposes the Original SQL API
 * (RistrettoDB: open/exec/query/close) and the append-only Table V2 API
 * (RistrettoTable: create/open/appendRow/getRowCount/close).
 *
 * Requires Node >= 18 and the `koffi` FFI package (prebuilt, no native build).
 * Build the shared library first:  make dynamic   (from the repo root).
 *
 * @example
 *   const { RistrettoTable, RistrettoValue } = require('./ristretto');
 *   const t = RistrettoTable.create('events',
 *     'CREATE TABLE events (timestamp INTEGER, event TEXT(32))');
 *   t.appendRow([RistrettoValue.integer(1672531200), RistrettoValue.text('login')]);
 *   t.close();
 */

'use strict';

const koffi = require('koffi');
const path = require('path');
const fs = require('fs');

// ---- Locate the shared library (relative to this file) ---------------------
function findLibrary() {
  const here = __dirname;
  const repoLib = path.join(here, '..', '..', 'lib');
  const names = ['libristretto.so', 'libristretto.dylib'];
  const candidates = [];
  for (const n of names) {
    candidates.push(path.join(repoLib, n));
    candidates.push(path.join(here, n));
  }
  candidates.push('/usr/local/lib/libristretto.so',
                  '/usr/local/lib/libristretto.dylib',
                  '/usr/lib/libristretto.so');
  for (const p of candidates) {
    if (fs.existsSync(p)) return p;
  }
  throw new Error(
    'Could not find libristretto shared library. Build it first:\n' +
    '  cd <repo root> && make dynamic\n' +
    '  (lib/libristretto.so on Linux, lib/libristretto.dylib on macOS)');
}

const libPath = findLibrary();
const lib = koffi.load(libPath);

// ---- Types -----------------------------------------------------------------
// RistrettoValue: { int type; union { int64 | double | {void* data; size_t len} }; bool is_null; }
const RText = koffi.struct('RText', { data: 'void *', length: 'size_t' });
const RValueUnion = koffi.union('RValueUnion', {
  integer: 'int64_t',
  real: 'double',
  text: RText,
});
const CValue = koffi.struct('RistrettoValueC', {
  type: 'int',
  value: RValueUnion,
  is_null: 'bool',
});

if (koffi.sizeof(CValue) !== 32) {
  throw new Error(`RistrettoValue ABI mismatch: expected 32 bytes, got ${koffi.sizeof(CValue)}`);
}

const QueryCallback = koffi.proto(
  'void RistrettoQueryCallback(void *ctx, int nCols, char **values, char **colNames)');

// ---- Function bindings -----------------------------------------------------
const c = {
  version: lib.func('const char *ristretto_version(void)'),
  versionNumber: lib.func('int ristretto_version_number(void)'),

  open: lib.func('void *ristretto_open(const char *filename)'),
  close: lib.func('void ristretto_close(void *db)'),
  exec: lib.func('int ristretto_exec(void *db, const char *sql)'),
  query: lib.func('int ristretto_query(void *db, const char *sql, RistrettoQueryCallback *cb, void *ctx)'),
  errorString: lib.func('const char *ristretto_error_string(int result)'),

  tableCreate: lib.func('void *ristretto_table_create(const char *name, const char *schema)'),
  tableOpen: lib.func('void *ristretto_table_open(const char *name)'),
  tableClose: lib.func('void ristretto_table_close(void *table)'),
  tableRowCount: lib.func('size_t ristretto_table_get_row_count(void *table)'),
  tableAppendRowN: lib.func('bool ristretto_table_append_row_n(void *table, RistrettoValueC *values, uint32_t n)'),
};

// ---- Enums / errors --------------------------------------------------------
const RistrettoResult = {
  OK: 0, ERROR: -1, NOMEM: -2, IO_ERROR: -3,
  PARSE_ERROR: -4, NOT_FOUND: -5, CONSTRAINT_ERROR: -6,
};

const RistrettoColumnType = { INTEGER: 1, REAL: 2, TEXT: 3, NULLABLE: 4 };

class RistrettoError extends Error {
  constructor(resultCode, message = '') {
    const name = Object.keys(RistrettoResult).find(k => RistrettoResult[k] === resultCode) || 'UNKNOWN';
    super(`RistrettoDB Error (${name}): ${message}`);
    this.name = 'RistrettoError';
    this.resultCode = resultCode;
  }
}

// ---- Value -----------------------------------------------------------------
class RistrettoValue {
  constructor(type, value, isNull = false) {
    this.type = type;
    this.value = value;
    this.isNull = isNull;
  }
  static integer(value) { return new RistrettoValue(RistrettoColumnType.INTEGER, value); }
  static real(value)    { return new RistrettoValue(RistrettoColumnType.REAL, value); }
  static text(value)    { return new RistrettoValue(RistrettoColumnType.TEXT, value); }
  static null()         { return new RistrettoValue(RistrettoColumnType.NULLABLE, null, true); }
  toString() {
    if (this.isNull) return 'RistrettoValue(NULL)';
    const t = Object.keys(RistrettoColumnType).find(k => RistrettoColumnType[k] === this.type);
    return `RistrettoValue(${t}, ${JSON.stringify(this.value)})`;
  }
}

// Build the C struct object for one value. For TEXT, returns a backing Buffer
// that must stay referenced until after the append call (the engine copies the
// bytes into the row during the call).
function toCValue(v, keepAlive) {
  if (v.isNull || v.type === RistrettoColumnType.NULLABLE) {
    return { type: RistrettoColumnType.NULLABLE, value: { integer: 0 }, is_null: true };
  }
  switch (v.type) {
    case RistrettoColumnType.INTEGER:
      return { type: v.type, value: { integer: Number(v.value) }, is_null: false };
    case RistrettoColumnType.REAL:
      return { type: v.type, value: { real: Number(v.value) }, is_null: false };
    case RistrettoColumnType.TEXT: {
      const buf = Buffer.from(String(v.value), 'utf8');
      keepAlive.push(buf);
      return { type: v.type, value: { text: { data: buf, length: buf.length } }, is_null: false };
    }
    default:
      throw new RistrettoError(RistrettoResult.ERROR, `Unsupported value type: ${v.type}`);
  }
}

// ---- Original SQL API ------------------------------------------------------
class RistrettoDB {
  constructor(filename) {
    this.filename = filename;
    this._handle = c.open(filename);
    if (!this._handle) {
      throw new RistrettoError(RistrettoResult.ERROR, `Failed to open database: ${filename}`);
    }
  }
  close() {
    if (this._handle) { c.close(this._handle); this._handle = null; }
  }
  exec(sql) {
    if (!this._handle) throw new RistrettoError(RistrettoResult.ERROR, 'Database is closed');
    const r = c.exec(this._handle, sql);
    if (r !== RistrettoResult.OK) throw new RistrettoError(r, c.errorString(r));
  }
  query(sql, callback) {
    if (!this._handle) throw new RistrettoError(RistrettoResult.ERROR, 'Database is closed');
    const results = [];
    const cb = koffi.register((ctx, nCols, valuesPtr, colNamesPtr) => {
      const values = nCols > 0 ? koffi.decode(valuesPtr, 'char *', nCols) : [];
      const colNames = nCols > 0 ? koffi.decode(colNamesPtr, 'char *', nCols) : [];
      const row = {};
      for (let i = 0; i < nCols; i++) row[colNames[i] ?? `col_${i}`] = values[i];
      if (callback) callback(row); else results.push(row);
    }, koffi.pointer(QueryCallback));
    try {
      const r = c.query(this._handle, sql, cb, null);
      if (r !== RistrettoResult.OK) throw new RistrettoError(r, c.errorString(r));
    } finally {
      koffi.unregister(cb);
    }
    return results;
  }
  static version() { return c.version(); }
  static versionNumber() { return c.versionNumber(); }
}

// ---- Table V2 API ----------------------------------------------------------
class RistrettoTable {
  constructor(handle, name) { this._handle = handle; this.name = name; }

  static create(name, schemaSql) {
    const h = c.tableCreate(name, schemaSql);
    if (!h) throw new RistrettoError(RistrettoResult.ERROR, `Failed to create table: ${name}`);
    return new RistrettoTable(h, name);
  }
  static open(name) {
    const h = c.tableOpen(name);
    if (!h) throw new RistrettoError(RistrettoResult.ERROR, `Failed to open table: ${name}`);
    return new RistrettoTable(h, name);
  }
  close() {
    if (this._handle) { c.tableClose(this._handle); this._handle = null; }
  }
  getRowCount() {
    if (!this._handle) throw new RistrettoError(RistrettoResult.ERROR, 'Table is closed');
    return Number(c.tableRowCount(this._handle));
  }
  appendRow(values) {
    if (!this._handle) throw new RistrettoError(RistrettoResult.ERROR, 'Table is closed');
    const keepAlive = [];
    const arr = values.map(v => toCValue(v, keepAlive));
    const ok = c.tableAppendRowN(this._handle, arr, arr.length);
    // keepAlive buffers stay referenced through this synchronous call.
    if (!ok) throw new RistrettoError(RistrettoResult.ERROR, `append_row failed for table '${this.name}'`);
    return true;
  }
}

module.exports = {
  RistrettoDB,
  RistrettoTable,
  RistrettoValue,
  RistrettoError,
  RistrettoResult,
  RistrettoColumnType,
  version: () => c.version(),
  versionNumber: () => c.versionNumber(),
  libraryPath: libPath,
};
