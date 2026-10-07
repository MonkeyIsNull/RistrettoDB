#!/usr/bin/env python3
"""
RistrettoDB Python Example (Table V2)

Shows how to use RistrettoDB from Python: create a fixed-schema table, append
rows (including a NULL), scan them back, and confirm they persist across a
close/reopen cycle.

Requirements:
    - Python 3.6+
    - RistrettoDB library built (run: cd ../../ && make dynamic)

Usage:
    python3 example.py
"""

import sys
import os

# Add current directory to path so we can import ristretto
sys.path.insert(0, os.path.dirname(__file__))

from ristretto import RistrettoTable, RistrettoValue, RistrettoError, version


def table_v2_api_example():
    """Demonstrate the Table V2 API with a sensor-telemetry workload."""
    print("Table V2 API Example")
    print("=" * 40)

    schema = """
        CREATE TABLE sensor_readings (
            timestamp INTEGER,
            device_id INTEGER,
            temperature REAL,
            location TEXT(16)
        )
    """

    locations = ["Kitchen", "Bedroom", "Garage", "Attic", "Basement"]
    base_timestamp = 1672531200  # 2023-01-01 00:00:00 UTC
    total_inserts = 100
    column_count = 4

    with RistrettoTable.create("sensor_data", schema) as table:
        print("SUCCESS: created 'sensor_readings' table")

        for i in range(total_inserts):
            # Every 10th reading has an unknown (NULL) location to show that
            # NULLs persist and round-trip as None.
            loc = RistrettoValue.null() if i % 10 == 0 \
                else RistrettoValue.text(locations[i % len(locations)])
            values = [
                RistrettoValue.integer(base_timestamp + i * 60),
                RistrettoValue.integer(i % 10),
                RistrettoValue.real(20.0 + (i % 25)),
                loc,
            ]
            table.append_row(values)

        count = table.get_row_count()
        print(f"SUCCESS: inserted {count} readings")
        if count != total_inserts:
            print("ERROR: row count does not match inserts")
            return False

        # Scan the rows back and confirm real values + a NULL round-trip.
        rows = table.scan()
        if len(rows) != total_inserts:
            print(f"ERROR: scan returned {len(rows)} rows, expected {total_inserts}")
            return False

        print("First 3 rows read back:")
        for cols in rows[:3]:
            print(f"   {cols}")

        null_rows = [r for r in rows if r[3] is None]
        if not null_rows:
            print("ERROR: expected at least one NULL location to round-trip as None")
            return False
        print(f"SUCCESS: {len(null_rows)} rows have a NULL location (round-tripped as None)")

    # Reopen the (now closed) table and confirm the rows persisted.
    with RistrettoTable.open("sensor_data", column_count=column_count) as reopened:
        if reopened.get_row_count() != total_inserts:
            print(f"ERROR: after reopen expected {total_inserts} rows, "
                  f"got {reopened.get_row_count()}")
            return False
        # Confirm the NULL survived the reopen too.
        reopened_rows = reopened.scan()
        if not any(r[3] is None for r in reopened_rows):
            print("ERROR: NULL location did not survive reopen")
            return False
        print(f"SUCCESS: reopened table, {reopened.get_row_count()} rows persisted "
              f"(NULL preserved)")

    print("SUCCESS: Table V2 API example completed successfully!\n")
    return True


def main():
    print("RistrettoDB Python Bindings Example (Table V2)")
    print("=" * 50)
    print("A fast, embeddable, append-only telemetry/analytics store")
    print("https://github.com/MonkeyIsNull/RistrettoDB")
    print()

    try:
        print(f"SUCCESS: RistrettoDB v{version()} loaded successfully\n")
    except Exception as e:  # noqa: BLE001
        print(f"ERROR: Failed to load RistrettoDB library: {e}")
        print("\nMake sure to build the library first:")
        print("   cd ../../ && make dynamic")
        return 1

    try:
        ok = table_v2_api_example()
    except RistrettoError as e:
        print(f"ERROR: {e}")
        return 1

    print("=" * 50)
    if ok:
        print("SUCCESS: Example completed successfully!")
        return 0
    print("WARNING: Example failed. Check error messages above.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
