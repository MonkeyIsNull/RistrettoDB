# RistrettoDB Go Bindings

cgo bindings for [RistrettoDB](../../), a fast, embeddable, fixed-schema,
append-only, single-writer telemetry/analytics store written in C.

This directory is a self-contained Go module exposing the **Table V2 API**
(`Table` / `CreateTable` / `OpenTable` / `AppendRow` / `Scan`): a fixed-width,
append-only, mmap-backed table store. RistrettoDB is not a general-purpose SQL
database — there is no query language, JOINs, UPDATE/DELETE, or transactions.

## Layout

```
examples/go/
├── go.mod                 module github.com/MonkeyIsNull/RistrettoDB/examples/go
├── ristretto/             the binding package
│   ├── ristretto.go       public API (Table, Value, ...)
│   ├── cbridge.h/.c       small C glue (cgo compiles cbridge.c automatically)
│   ├── exports.go         //export scan callback
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

These reflect the design of the underlying C engine, not the bindings:

- **NULLs persist and round-trip as NULL.** A value written with `NullValue()`
  reads back with `IsNull` set (format v3 stores a per-row NULL bitmap).
- **No `WHERE` clause.** `Scan` / `ForEach` return every row; filter in Go.
- **Single-writer.** An advisory `flock` guards the file (a no-op on some network
  filesystems); open a table from one writer at a time.
- **No WAL.** `Close` does a durable `msync`+`fsync`, but a crash mid-run may lose
  rows written since the last sync.
