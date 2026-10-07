// Command example is a small, self-contained demo of the RistrettoDB Go
// bindings: it creates a Table V2 table, appends rows (including TEXT and
// NULL), reads them back, and demonstrates that the data survives a
// close/reopen cycle.
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

	v2Demo()
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
		name := row.Get("name")
		nameStr := fmt.Sprintf("%q", name.Text())
		if name.IsNull {
			nameStr = "NULL"
		}
		fmt.Printf("    ts=%d name=%s value=%v\n",
			row.Get("ts").Int(), nameStr, row.Get("value").Float())
	}
}
