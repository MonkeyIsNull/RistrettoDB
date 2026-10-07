// Command example is a small, self-contained demo of the RistrettoDB Go
// bindings: it opens a database, uses both the Original SQL API and the
// Table V2 ultra-fast API, appends rows (including TEXT and NULL), reads them
// back, and demonstrates that V2 data survives a close/reopen cycle.
//
// Run it from the examples/go module directory after building the C library:
//
//	cd ../../ && make static        # from this file: the repo root
//	go run ./cmd/example            # from examples/go
package main

import (
	"fmt"
	"log"

	"github.com/MonkeyIsNull/RistrettoDB/examples/go/ristretto"
)

func main() {
	fmt.Printf("RistrettoDB v%s (version number %d)\n\n",
		ristretto.Version(), ristretto.VersionNumber())

	sqlDemo()
	v2Demo()
}

func sqlDemo() {
	fmt.Println("== Original SQL API ==")
	db, err := ristretto.Open("example_sql.db")
	if err != nil {
		log.Fatalf("open: %v", err)
	}
	defer db.Close()

	if err := db.Exec("CREATE TABLE users (id INTEGER, name TEXT)"); err != nil {
		log.Fatalf("create: %v", err)
	}
	// NOTE: the C SQL parser does not support escaped quotes, so TEXT literals
	// cannot contain a single quote. QuoteString still guards the common case.
	for _, u := range []struct {
		id   int
		name string
	}{{1, "Alice"}, {2, "Bob"}, {3, "Carol"}} {
		sql := fmt.Sprintf("INSERT INTO users VALUES (%d, %s)", u.id, ristretto.QuoteString(u.name))
		if err := db.Exec(sql); err != nil {
			log.Fatalf("insert: %v", err)
		}
	}

	rows, err := db.Query("SELECT * FROM users")
	if err != nil {
		log.Fatalf("query: %v", err)
	}
	for _, r := range rows {
		fmt.Printf("  id=%s name=%s\n", r["id"], r["name"])
	}
	fmt.Println()
}

func v2Demo() {
	fmt.Println("== Table V2 ultra-fast API ==")
	schema := "CREATE TABLE events (ts INTEGER, name TEXT(16), value REAL)"

	tbl, err := ristretto.CreateTable("events", schema)
	if err != nil {
		log.Fatalf("create table: %v", err)
	}

	rows := [][]ristretto.Value{
		{ristretto.IntegerValue(1001), ristretto.TextValue("login"), ristretto.RealValue(1.5)},
		{ristretto.IntegerValue(1002), ristretto.TextValue("click"), ristretto.RealValue(2.25)},
		{ristretto.IntegerValue(1003), ristretto.NullValue(), ristretto.RealValue(3.0)},
	}
	for _, r := range rows {
		if err := tbl.AppendRow(r); err != nil {
			log.Fatalf("append: %v", err)
		}
	}
	fmt.Printf("  appended %d rows (row count: %d)\n", len(rows), tbl.RowCount())
	tbl.Close()

	// Reopen to prove the data survived.
	reopened, err := ristretto.OpenTable("events")
	if err != nil {
		log.Fatalf("reopen: %v", err)
	}
	defer reopened.Close()

	fmt.Printf("  reopened table has %d rows:\n", reopened.RowCount())
	scanned, err := reopened.Scan()
	if err != nil {
		log.Fatalf("scan: %v", err)
	}
	for _, row := range scanned {
		fmt.Printf("    ts=%d name=%q value=%v\n",
			row.Get("ts").Int(), row.Get("name").Text(), row.Get("value").Float())
	}
}
