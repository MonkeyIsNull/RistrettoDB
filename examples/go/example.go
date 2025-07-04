package main

import (
	"fmt"
	"log"
	"math/rand"
	"strings"
	"time"

	"ristrettodb-go-bindings"
)

// originalSqlApiExample demonstrates the Original SQL API (2.8x faster than SQLite)
func originalSqlApiExample() bool {
	fmt.Println("🔧 Original SQL API Example")
	fmt.Println(strings.Repeat("=", 40))

	// Open database
	db, err := ristretto.Open("go_sql_example.db")
	if err != nil {
		fmt.Printf("❌ Failed to open database: %v\n", err)
		return false
	}
	defer db.Close()

	fmt.Printf("✅ Connected to RistrettoDB v%s\n", ristretto.Version())

	// Create a table for e-commerce orders
	err = db.Exec(`
		CREATE TABLE orders (
			id INTEGER,
			customer_name TEXT,
			product TEXT,
			quantity INTEGER,
			total_price REAL
		)
	`)
	if err != nil {
		fmt.Printf("❌ Failed to create table: %v\n", err)
		return false
	}
	fmt.Println("✅ Created 'orders' table")

	// Insert sample order data
	orders := []struct {
		id           int
		customerName string
		product      string
		quantity     int
		totalPrice   float64
	}{
		{1, "John Smith", "Laptop Pro", 1, 1299.99},
		{2, "Sarah Johnson", "Wireless Headphones", 2, 159.98},
		{3, "Mike Brown", "Gaming Mouse", 3, 89.97},
		{4, "Emily Davis", "4K Monitor", 1, 399.99},
		{5, "David Wilson", "Mechanical Keyboard", 1, 149.99},
		{6, "Lisa Anderson", "USB-C Hub", 2, 79.98},
	}

	for _, order := range orders {
		sql := fmt.Sprintf(
			"INSERT INTO orders VALUES (%d, '%s', '%s', %d, %.2f)",
			order.id, order.customerName, order.product, order.quantity, order.totalPrice,
		)
		err = db.Exec(sql)
		if err != nil {
			fmt.Printf("❌ Failed to insert order %d: %v\n", order.id, err)
			return false
		}
	}
	fmt.Printf("✅ Inserted %d orders\n", len(orders))

	// Query all orders
	fmt.Println("\n📦 All orders:")
	results, err := db.Query("SELECT * FROM orders")
	if err != nil {
		fmt.Printf("❌ Query failed: %v\n", err)
		return false
	}

	for _, row := range results {
		fmt.Printf("   Order %s: %s ordered %s (qty: %s) - $%s\n",
			row["id"], row["customer_name"], row["product"],
			row["quantity"], row["total_price"])
	}

	// Query high-value orders
	fmt.Println("\n💰 High-value orders (> $200):")
	highValueOrders, err := db.Query("SELECT customer_name, product, total_price FROM orders WHERE total_price > 200")
	if err != nil {
		fmt.Printf("❌ High-value query failed: %v\n", err)
		return false
	}

	for _, row := range highValueOrders {
		fmt.Printf("   %s: %s - $%s\n", row["customer_name"], row["product"], row["total_price"])
	}

	// Calculate total sales
	totalSales, err := db.Query("SELECT SUM(total_price) as total FROM orders")
	if err != nil {
		fmt.Printf("❌ Total sales query failed: %v\n", err)
		return false
	}

	if len(totalSales) > 0 {
		fmt.Printf("\n💵 Total sales: $%s\n", totalSales[0]["total"])
	}

	fmt.Printf("\n✅ Found %d high-value orders\n", len(highValueOrders))
	fmt.Println("✅ Original SQL API example completed successfully!\n")
	return true
}

// tableV2ApiExample demonstrates the Table V2 Ultra-Fast API (4.57x faster than SQLite)
func tableV2ApiExample() bool {
	fmt.Println("⚡ Table V2 Ultra-Fast API Example")
	fmt.Println(strings.Repeat("=", 40))

	// Create ultra-fast table for financial trading data
	schema := `
		CREATE TABLE trading_data (
			timestamp INTEGER,
			symbol TEXT(8),
			price REAL,
			volume INTEGER,
			exchange TEXT(16)
		)
	`

	table, err := ristretto.CreateTable("trading_stream", schema)
	if err != nil {
		fmt.Printf("❌ Failed to create table: %v\n", err)
		return false
	}
	defer table.Close()

	fmt.Println("✅ Created ultra-fast 'trading_data' table")
	fmt.Println("   Optimized for 4.6M+ rows/second throughput")

	// Simulate high-frequency trading data ingestion
	fmt.Println("\n📈 Simulating high-frequency trading data...")

	symbols := []string{"AAPL", "GOOGL", "MSFT", "AMZN", "TSLA", "META", "NVDA", "NFLX"}
	exchanges := []string{"NASDAQ", "NYSE", "BATS", "ARCA"}
	baseTimestamp := time.Now().Unix()

	successfulInserts := 0
	totalInserts := 200 // Simulate 200 trading events

	rand.Seed(time.Now().UnixNano())

	for i := 0; i < totalInserts; i++ {
		// Generate realistic trading data
		symbol := symbols[rand.Intn(len(symbols))]
		basePrice := float64(100 + rand.Intn(300)) // Price between $100-$400
		priceVariation := (rand.Float64() - 0.5) * 10 // ±$5 variation
		price := basePrice + priceVariation
		volume := rand.Intn(10000) + 100 // Volume between 100-10100
		exchange := exchanges[rand.Intn(len(exchanges))]

		values := []ristretto.Value{
			ristretto.IntegerValue(baseTimestamp + int64(i)),
			ristretto.TextValue(symbol),
			ristretto.RealValue(price),
			ristretto.IntegerValue(int64(volume)),
			ristretto.TextValue(exchange),
		}

		// High-speed append (optimized for performance)
		err = table.AppendRow(values)
		if err != nil {
			fmt.Printf("   ⚠️  Failed to insert trade %d: %v\n", i, err)
		} else {
			successfulInserts++
		}

		// Simulate realistic timing (every few microseconds)
		if i%50 == 0 {
			time.Sleep(1 * time.Microsecond)
		}
	}

	fmt.Println("✅ High-frequency ingestion completed")
	fmt.Printf("   Successfully processed: %d/%d trades\n", successfulInserts, totalInserts)
	fmt.Printf("   Total rows in table: %d\n", table.GetRowCount())

	// Performance and use case summary
	fmt.Println("\n📊 Performance Summary:")
	fmt.Printf("   • Trade processing rate: ~%d events simulated\n", totalInserts)
	fmt.Printf("   • Memory efficient: Fixed-width row format\n")
	fmt.Printf("   • Zero-copy I/O: Memory-mapped file access\n")
	fmt.Printf("   • Append-only: Optimized for write-heavy trading workloads\n")

	fmt.Println("\n🏦 Financial Trading Use Cases:")
	fmt.Println("   • High-frequency trading data capture")
	fmt.Println("   • Real-time market data feeds")
	fmt.Println("   • Order book snapshots")
	fmt.Println("   • Trade execution logging")
	fmt.Println("   • Risk management data")
	fmt.Println("   • Compliance and audit trails")

	fmt.Println("✅ Table V2 ultra-fast API example completed successfully!\n")
	return true
}

// integrationExamples shows practical integration examples
func integrationExamples() {
	fmt.Println("🔗 Integration Examples")
	fmt.Println(strings.Repeat("=", 30))

	fmt.Println("Perfect use cases for RistrettoDB Go bindings:\n")

	examples := []struct {
		category    string
		description string
	}{
		{"🌐 Web Services", "High-performance REST APIs, GraphQL backends"},
		{"🏗️  Microservices", "Service mesh data, inter-service communication"},
		{"📊 Analytics", "Real-time data processing, time-series analysis"},
		{"🤖 ML/AI Systems", "Feature stores, training data pipelines"},
		{"🏭 IoT Platforms", "Device telemetry, sensor data aggregation"},
		{"🔒 Security Tools", "Log analysis, threat detection, audit systems"},
		{"🎮 Game Backends", "Player stats, match data, leaderboards"},
		{"💰 FinTech", "Transaction processing, trading systems, risk analysis"},
		{"🚀 DevOps Tools", "Monitoring systems, CI/CD pipelines, metrics"},
		{"📱 Mobile Backends", "User data sync, analytics, push notifications"},
	}

	for _, example := range examples {
		fmt.Printf("   %s\n", example.category)
		fmt.Printf("     └─ %s\n", example.description)
	}

	fmt.Println("\n📦 Installation:")
	fmt.Println("   1. Build RistrettoDB: cd ../../ && make lib")
	fmt.Println("   2. Copy bindings: cp examples/go/* your_project/")
	fmt.Println("   3. go mod init your-project")
	fmt.Println("   4. import \"./ristretto\"")

	fmt.Println("\n🚀 Performance Benefits:")
	fmt.Println("   • 2.8x faster than SQLite (Original API)")
	fmt.Println("   • 4.57x faster than SQLite (Table V2 API)")
	fmt.Println("   • Native Go integration with cgo")
	fmt.Println("   • Memory-safe resource management")
	fmt.Println("   • Concurrent access support")
	fmt.Println("   • Zero external dependencies")

	fmt.Println("\n💡 Go-Specific Advantages:")
	fmt.Println("   • Excellent for concurrent applications")
	fmt.Println("   • Perfect for microservices architectures")
	fmt.Println("   • Great for cloud-native applications")
	fmt.Println("   • Ideal for container-based deployments")
	fmt.Println("   • Seamless integration with Go ecosystems")
}

func main() {
	fmt.Println("🔵 RistrettoDB Go Bindings Example")
	fmt.Println(strings.Repeat("=", 50))
	fmt.Println("A tiny, blazingly fast, embeddable SQL engine")
	fmt.Println("https://github.com/MonkeyIsNull/RistrettoDB")
	fmt.Println()

	// Check if library is available
	version := ristretto.Version()
	if version == "" {
		fmt.Println("❌ Failed to load RistrettoDB library")
		fmt.Println("\n💡 Make sure to build the library first:")
		fmt.Println("   cd ../../ && make lib")
		fmt.Println("\n💡 Make sure cgo can find the library:")
		fmt.Println("   export CGO_LDFLAGS=\"-L../../lib\"")
		fmt.Println("   export LD_LIBRARY_PATH=\"../../lib:$LD_LIBRARY_PATH\"")
		return
	}

	fmt.Printf("✅ RistrettoDB v%s loaded successfully\n", version)
	fmt.Printf("   Version Number: %d\n", ristretto.VersionNumber())
	fmt.Println()

	// Run examples
	success := true

	if !originalSqlApiExample() {
		success = false
	}

	if !tableV2ApiExample() {
		success = false
	}

	integrationExamples()

	fmt.Println("\n" + strings.Repeat("=", 50))
	if success {
		fmt.Println("🎉 All examples completed successfully!")
		fmt.Println("   Ready to integrate RistrettoDB into your Go applications!")
		fmt.Println()
		fmt.Println("💻 Next Steps:")
		fmt.Println("   • Copy ristretto.go to your project")
		fmt.Println("   • Set up CGO_LDFLAGS for your build")
		fmt.Println("   • Start building high-performance Go applications!")
		fmt.Println()
		fmt.Println("🔧 Build Tips:")
		fmt.Println("   • go build -ldflags \"-L../../lib\" example.go")
		fmt.Println("   • Ensure libristretto.so is in your library path")
		fmt.Println("   • Use go mod for dependency management")
	} else {
		fmt.Println("⚠️  Some examples failed. Check error messages above.")
		log.Fatal("Example execution failed")
	}
}

