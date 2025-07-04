#!/usr/bin/env python3
"""
RistrettoDB Python Example

Simple example showing how to use RistrettoDB with Python.
Demonstrates both the Original SQL API and Table V2 Ultra-Fast API.

Requirements:
    - Python 3.6+
    - RistrettoDB library built (run: cd ../../ && make lib)

Usage:
    python3 example.py
"""

import sys
import os

# Add current directory to path so we can import ristretto
sys.path.insert(0, os.path.dirname(__file__))

from ristretto import RistrettoDB, RistrettoTable, RistrettoValue, RistrettoError

def original_sql_api_example():
    """Demonstrate the Original SQL API (2.8x faster than SQLite)"""
    print("Original SQL API Example")
    print("=" * 40)
    
    try:
        # Open database using context manager (auto-close)
        with RistrettoDB("python_sql_example.db") as db:
            print(f"SUCCESS: Connected to RistrettoDB v{db.version()}")
            
            # Create a table for storing user data
            db.exec("""
                CREATE TABLE users (
                    id INTEGER,
                    username TEXT,
                    score REAL
                )
            """)
            print("SUCCESS: Created 'users' table")
            
            # Insert some sample data
            users = [
                (1, "alice", 95.5),
                (2, "bob", 87.2),
                (3, "charlie", 92.8),
                (4, "diana", 98.1)
            ]
            
            for user_id, username, score in users:
                db.exec(f"INSERT INTO users VALUES ({user_id}, '{username}', {score})")
            
            print(f"SUCCESS: Inserted {len(users)} users")
            
            # Query all users
            print("\nAll users:")
            results = db.query("SELECT * FROM users")
            for row in results:
                print(f"   ID: {row['id']}, Username: {row['username']}, Score: {row['score']}")
            
            # Query high-scoring users
            print("\nHigh-scoring users (score > 90):")
            high_scorers = db.query("SELECT username, score FROM users WHERE score > 90")
            for row in high_scorers:
                print(f"   {row['username']}: {row['score']}")
            
            print(f"\nSUCCESS: Found {len(high_scorers)} high-scoring users")
    
    except RistrettoError as e:
        print(f"ERROR: Database error: {e}")
        return False
    except Exception as e:
        print(f"ERROR: Unexpected error: {e}")
        return False
    
    print("SUCCESS: Original SQL API example completed successfully!\n")
    return True

def table_v2_api_example():
    """Demonstrate the Table V2 Ultra-Fast API (4.57x faster than SQLite)"""
    print("Table V2 Ultra-Fast API Example")
    print("=" * 40)
    
    try:
        # Create ultra-fast table for IoT sensor data
        schema = """
            CREATE TABLE sensor_readings (
                timestamp INTEGER,
                device_id INTEGER,
                temperature REAL,
                humidity REAL,
                location TEXT(16)
            )
        """
        
        with RistrettoTable.create("sensor_data", schema) as table:
            print("SUCCESS: Created ultra-fast 'sensor_readings' table")
            print(f"   Optimized for 4.6M+ rows/second throughput")
            
            # Simulate high-speed sensor data ingestion
            print("\nSimulating IoT sensor data ingestion...")
            
            # Sample sensor locations
            locations = ["Kitchen", "Bedroom", "Garage", "Attic", "Basement"]
            base_timestamp = 1672531200  # 2023-01-01 00:00:00 UTC
            
            successful_inserts = 0
            total_inserts = 100  # Simulate 100 sensor readings
            
            for i in range(total_inserts):
                try:
                    # Create sensor reading values
                    values = [
                        RistrettoValue.integer(base_timestamp + i * 60),  # Every minute
                        RistrettoValue.integer(i % 10),                  # Device ID 0-9
                        RistrettoValue.real(20.0 + (i % 25)),           # Temperature 20-45°C
                        RistrettoValue.real(40.0 + (i % 40)),           # Humidity 40-80%
                        RistrettoValue.text(locations[i % len(locations)])  # Location
                    ]
                    
                    # High-speed append (optimized for performance)
                    if table.append_row(values):
                        successful_inserts += 1
                    
                    # Clean up text values to prevent memory leaks
                    # (In the actual implementation, this would be handled automatically)
                    
                except Exception as e:
                    print(f"   WARNING: Failed to insert reading {i}: {e}")
            
            print(f"SUCCESS: High-speed ingestion completed")
            print(f"   Successfully inserted: {successful_inserts}/{total_inserts} readings")
            print(f"   Total rows in table: {table.get_row_count()}")
            
            # Performance summary
            print(f"\nPerformance Summary:")
            print(f"   • Insertion rate: ~{total_inserts} readings simulated")
            print(f"   • Memory efficient: Fixed-width row format")
            print(f"   • Zero-copy I/O: Memory-mapped file access")
            print(f"   • Append-only: Optimized for write-heavy workloads")
    
    except RistrettoError as e:
        print(f"ERROR: Table error: {e}")
        return False
    except Exception as e:
        print(f"ERROR: Unexpected error: {e}")
        return False
    
    print("SUCCESS: Table V2 ultra-fast API example completed successfully!\n")
    return True

def integration_examples():
    """Show practical integration examples"""
    print("Integration Examples")
    print("=" * 30)
    
    print("Perfect use cases for RistrettoDB Python bindings:\n")
    
    examples = [
        ("Web Applications", "High-speed session storage, user analytics"),
        ("Data Analytics", "Real-time event ingestion, time-series data"),
        ("Machine Learning", "Feature storage, training data preparation"),
        ("IoT Systems", "Sensor data collection, device telemetry"),
        ("Security", "Audit trails, tamper-evident logging"),
        ("Gaming", "Player statistics, match analytics"),
        ("Finance", "Trading data, transaction logging"),
        ("Mobile Apps", "Offline data sync, local analytics")
    ]
    
    for category, description in examples:
        print(f"   {category}")
        print(f"     └─ {description}")
    
    print("\nInstallation:")
    print("   1. Build RistrettoDB: cd ../../ && make lib")
    print("   2. Copy bindings: cp examples/python/ristretto.py your_project/")
    print("   3. Use in your code: from ristretto import RistrettoDB")
    
    print("\nPerformance Benefits:")
    print("   • 2.8x faster than SQLite (Original API)")
    print("   • 4.57x faster than SQLite (Table V2 API)")
    print("   • Zero external dependencies")
    print("   • Thread-safe operations")
    print("   • Memory-efficient design")

def main():
    """Main example runner"""
    print("RistrettoDB Python Bindings Example")
    print("=" * 50)
    print("A tiny, blazingly fast, embeddable SQL engine")
    print("https://github.com/MonkeyIsNull/RistrettoDB")
    print()
    
    # Check if library is available
    try:
        version = RistrettoDB.version()
        print(f"SUCCESS: RistrettoDB v{version} loaded successfully")
        print()
    except Exception as e:
        print(f"ERROR: Failed to load RistrettoDB library: {e}")
        print("\nMake sure to build the library first:")
        print("   cd ../../ && make lib")
        return 1
    
    # Run examples
    success = True
    
    success &= original_sql_api_example()
    success &= table_v2_api_example()
    
    integration_examples()
    
    print("\n" + "=" * 50)
    if success:
        print("SUCCESS: All examples completed successfully!")
        print("   Ready to integrate RistrettoDB into your Python applications!")
    else:
        print("WARNING: Some examples failed. Check error messages above.")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())