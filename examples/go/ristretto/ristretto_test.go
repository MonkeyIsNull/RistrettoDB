package ristretto

import (
	"os"
	"testing"
)

// chdirTemp moves into a fresh temp dir for the duration of a test so that the
// "data/<name>.rdb" files Table V2 writes are isolated and cleaned up.
func chdirTemp(t *testing.T) {
	t.Helper()
	orig, err := os.Getwd()
	if err != nil {
		t.Fatalf("getwd: %v", err)
	}
	dir := t.TempDir()
	if err := os.Chdir(dir); err != nil {
		t.Fatalf("chdir: %v", err)
	}
	t.Cleanup(func() { _ = os.Chdir(orig) })
}

func TestVersion(t *testing.T) {
	if Version() == "" {
		t.Fatal("empty version string")
	}
	if VersionNumber() == 0 {
		t.Fatal("zero version number")
	}
	t.Logf("RistrettoDB %s (%d)", Version(), VersionNumber())
}

func TestQuoteString(t *testing.T) {
	if got := QuoteString("O'Brien"); got != "'O''Brien'" {
		t.Fatalf("QuoteString: got %q", got)
	}
}

func TestV2AppendAndScan(t *testing.T) {
	chdirTemp(t)

	schema := "CREATE TABLE events (ts INTEGER, name TEXT(16), value REAL)"
	tbl, err := CreateTable("events", schema)
	if err != nil {
		t.Fatalf("CreateTable: %v", err)
	}
	defer tbl.Close()

	// Schema was parsed from C header.
	cols := tbl.Columns()
	if len(cols) != 3 {
		t.Fatalf("expected 3 columns, got %d: %+v", len(cols), cols)
	}
	if cols[0].Name != "ts" || cols[0].Type != INTEGER {
		t.Errorf("col0 = %+v, want {ts INTEGER}", cols[0])
	}
	if cols[1].Name != "name" || cols[1].Type != TEXT {
		t.Errorf("col1 = %+v, want {name TEXT}", cols[1])
	}
	if cols[2].Name != "value" || cols[2].Type != REAL {
		t.Errorf("col2 = %+v, want {value REAL}", cols[2])
	}

	rows := [][]Value{
		{IntegerValue(1001), TextValue("login"), RealValue(1.5)},
		{IntegerValue(1002), TextValue("click"), RealValue(2.25)},
		{IntegerValue(1003), NullValue(), RealValue(3.0)}, // NULL text
	}
	for i, r := range rows {
		if err := tbl.AppendRow(r); err != nil {
			t.Fatalf("AppendRow[%d]: %v", i, err)
		}
	}

	if got := tbl.RowCount(); got != 3 {
		t.Fatalf("RowCount = %d, want 3", got)
	}

	scanned, err := tbl.Scan()
	if err != nil {
		t.Fatalf("Scan: %v", err)
	}
	if len(scanned) != 3 {
		t.Fatalf("Scan returned %d rows, want 3", len(scanned))
	}

	// Row 0: full values.
	if got := scanned[0].Get("ts").Int(); got != 1001 {
		t.Errorf("row0 ts = %d, want 1001", got)
	}
	if got := scanned[0].Get("name").Text(); got != "login" {
		t.Errorf("row0 name = %q, want login", got)
	}
	if got := scanned[0].Get("value").Float(); got != 1.5 {
		t.Errorf("row0 value = %v, want 1.5", got)
	}

	// Row 1.
	if got := scanned[1].Get("name").Text(); got != "click" {
		t.Errorf("row1 name = %q, want click", got)
	}
	if got := scanned[1].Get("value").Float(); got != 2.25 {
		t.Errorf("row1 value = %v, want 2.25", got)
	}

	// Row 2: the NULL text reads back as "" (V2 does not persist NULL-ness).
	if got := scanned[2].Get("ts").Int(); got != 1003 {
		t.Errorf("row2 ts = %d, want 1003", got)
	}
	if got := scanned[2].Get("name").Text(); got != "" {
		t.Errorf("row2 name = %q, want empty (NULL not persisted)", got)
	}
	if got := scanned[2].Get("value").Float(); got != 3.0 {
		t.Errorf("row2 value = %v, want 3.0", got)
	}
}

func TestV2ForEachEarlyStop(t *testing.T) {
	chdirTemp(t)

	tbl, err := CreateTable("nums", "CREATE TABLE nums (n INTEGER)")
	if err != nil {
		t.Fatalf("CreateTable: %v", err)
	}
	defer tbl.Close()

	for i := int64(0); i < 10; i++ {
		if err := tbl.AppendRow([]Value{IntegerValue(i)}); err != nil {
			t.Fatalf("AppendRow: %v", err)
		}
	}

	var seen []int64
	err = tbl.ForEach(func(r Row) bool {
		seen = append(seen, r.Get("n").Int())
		return len(seen) < 3 // stop after 3
	})
	if err != nil {
		t.Fatalf("ForEach: %v", err)
	}
	if len(seen) != 3 {
		t.Fatalf("early stop failed: visited %d rows %v", len(seen), seen)
	}
}

func TestV2WrongColumnCount(t *testing.T) {
	chdirTemp(t)

	tbl, err := CreateTable("t", "CREATE TABLE t (a INTEGER, b INTEGER)")
	if err != nil {
		t.Fatalf("CreateTable: %v", err)
	}
	defer tbl.Close()

	if err := tbl.AppendRow([]Value{IntegerValue(1)}); err == nil {
		t.Fatal("expected error for wrong column count, got nil")
	}
}

// TestV2ReopenSurvives is the restart-survival property: data written and
// closed must still be present after reopening the same table.
func TestV2ReopenSurvives(t *testing.T) {
	chdirTemp(t)

	schema := "CREATE TABLE persist (id INTEGER, label TEXT(32), score REAL)"
	tbl, err := CreateTable("persist", schema)
	if err != nil {
		t.Fatalf("CreateTable: %v", err)
	}

	want := [][]Value{
		{IntegerValue(10), TextValue("alpha"), RealValue(1.1)},
		{IntegerValue(20), TextValue("beta"), RealValue(2.2)},
		{IntegerValue(30), TextValue("gamma"), RealValue(3.3)},
		{IntegerValue(40), TextValue("delta"), RealValue(4.4)},
		{IntegerValue(50), TextValue("epsilon"), RealValue(5.5)},
	}
	for i, r := range want {
		if err := tbl.AppendRow(r); err != nil {
			t.Fatalf("AppendRow[%d]: %v", i, err)
		}
	}
	if got := tbl.RowCount(); got != int64(len(want)) {
		t.Fatalf("RowCount = %d, want %d", got, len(want))
	}

	// Close (flushes + unmaps), simulating process shutdown.
	if err := tbl.Close(); err != nil {
		t.Fatalf("Close: %v", err)
	}

	// Reopen from disk.
	reopened, err := OpenTable("persist")
	if err != nil {
		t.Fatalf("OpenTable: %v", err)
	}
	defer reopened.Close()

	if got := reopened.RowCount(); got != int64(len(want)) {
		t.Fatalf("after reopen RowCount = %d, want %d", got, len(want))
	}

	scanned, err := reopened.Scan()
	if err != nil {
		t.Fatalf("Scan: %v", err)
	}
	if len(scanned) != len(want) {
		t.Fatalf("after reopen Scan returned %d rows, want %d", len(scanned), len(want))
	}
	for i := range want {
		wantID := want[i][0].Int()
		wantLabel := want[i][1].Text()
		wantScore := want[i][2].Float()
		if got := scanned[i].Get("id").Int(); got != wantID {
			t.Errorf("row%d id = %d, want %d", i, got, wantID)
		}
		if got := scanned[i].Get("label").Text(); got != wantLabel {
			t.Errorf("row%d label = %q, want %q", i, got, wantLabel)
		}
		if got := scanned[i].Get("score").Float(); got != wantScore {
			t.Errorf("row%d score = %v, want %v", i, got, wantScore)
		}
	}
}

func TestSQLApi(t *testing.T) {
	chdirTemp(t)

	db, err := Open("sql_test.db")
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	defer db.Close()

	if err := db.Exec("CREATE TABLE users (id INTEGER, name TEXT)"); err != nil {
		t.Fatalf("Exec create: %v", err)
	}
	for _, s := range []string{
		"INSERT INTO users VALUES (1, 'Alice')",
		"INSERT INTO users VALUES (2, 'Bob')",
	} {
		if err := db.Exec(s); err != nil {
			t.Fatalf("Exec insert: %v", err)
		}
	}

	rows, err := db.Query("SELECT * FROM users")
	if err != nil {
		t.Fatalf("Query: %v", err)
	}
	if len(rows) != 2 {
		t.Fatalf("Query returned %d rows, want 2", len(rows))
	}
	if rows[0]["name"] != "Alice" || rows[1]["name"] != "Bob" {
		t.Errorf("unexpected rows: %+v", rows)
	}
}
