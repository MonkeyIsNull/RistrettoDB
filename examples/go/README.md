# RistrettoDB Go Bindings

cgo bindings for [RistrettoDB](../../), a tiny, embeddable database written in C.

This directory is a self-contained Go module exposing two APIs:

- **Original SQL API** (`DB` / `Open` / `Exec` / `Query`) — a small SQL engine
  supporting a limited subset of SQL (`CREATE TABLE`, `INSERT`, `SELECT`). It has
  no bound parameters, so statements are plain strings.
- **Table V2 API** (`Table` / `CreateTable` / `OpenTable` / `AppendRow` / `Scan`)
  — a fixed-width, append-only, mmap-backed table store. This is the fast
  write/scan path and the most complete part of the bindings.

## Layout

```
examples/go/
├── go.mod                 module github.com/MonkeyIsNull/RistrettoDB/examples/go
├── ristretto/             the binding package
│   ├── ristretto.go       public API (DB, Table, Value, ...)
│   ├── cbridge.h/.c       small C glue (cgo compiles cbridge.c automatically)
│   ├── exports.go         //export callbacks for query/scan
│   └── ristretto_test.go  tests
└── cmd/example/main.go    runnable demo
```

## Build & run

First build the C static library from the repository root:

```bash
cd ../../          # repository root
make static        # produces lib/libristretto.a
```

Then, from this module directory (`examples/go`):

```bash
go build ./...
go vet ./...
go test ./ristretto
go run ./cmd/example
```

The cgo directives in `ristretto/ristretto.go` link `lib/libristretto.a` by path
(relative to the source file via `${SRCDIR}`), so the resulting binaries have no
runtime dependency on `libristretto.so`.

## Example

```go
package main

import (
	"fmt"
	"log"

	"github.com/MonkeyIsNull/RistrettoDB/examples/go/ristretto"
)

func main() {
	// Table V2: create, append, read back.
	t, err := ristretto.CreateTable("events",
		"CREATE TABLE events (ts INTEGER, name TEXT(16), value REAL)")
	if err != nil {
		log.Fatal(err)
	}
	defer t.Close()

	_ = t.AppendRow([]ristretto.Value{
		ristretto.IntegerValue(1001),
		ristretto.TextValue("login"),
		ristretto.RealValue(1.5),
	})

	rows, _ := t.Scan()
	for _, r := range rows {
		fmt.Println(r.Get("ts").Int(), r.Get("name").Text(), r.Get("value").Float())
	}
}
```

A Table V2 table named `foo` is stored at `data/foo.rdb` relative to the current
working directory. `CreateTable` truncates any existing file; `OpenTable` resumes
one, and its rows survive process restarts.

## Known limitations

These reflect the current state of the underlying C engine, not the bindings:

- **V2 does not persist NULL-ness.** A value written with `NullValue()` reads back
  as the column's zero value (`0`, `0.0`, or `""`).
- **V2 ignores `WHERE`.** `table_select` in C has a `TODO` for `WHERE`, so `Scan`
  / `ForEach` return every row; filter in Go.
- **SQL API has no bound parameters** and its parser does not accept escaped
  quotes, so TEXT literals cannot contain a single quote. Use `QuoteString` for
  the common (quote-free) case.
- The SQL `Query` result is a `map[string]string` per row (column order is not
  preserved).
