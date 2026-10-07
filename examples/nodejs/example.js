#!/usr/bin/env node
/**
 * RistrettoDB Node.js Example
 * 
 * Simple example showing how to use RistrettoDB with Node.js.
 * Demonstrates both the Original SQL API and Table V2 Ultra-Fast API.
 * 
 * Requirements:
 *   - Node.js 12+
 *   - RistrettoDB library built (run: cd ../../ && make libraries)
 *   - Dependencies installed (run: npm install)
 * 
 * Usage:
 *   node example.js
 */

const { RistrettoDB, RistrettoTable, RistrettoValue, RistrettoError } = require('./ristretto');

function originalSqlApiExample() {
  console.log('[CONFIG] Original SQL API Example');
  console.log('=' .repeat(40));

  try {
    // Create database instance
    const db = new RistrettoDB('nodejs_sql_example.db');
    console.log(`[SUCCESS] Connected to RistrettoDB v${RistrettoDB.version()}`);

    // Create a table for storing product data
    db.exec(`
      CREATE TABLE products (
        id INTEGER,
        name TEXT,
        price REAL,
        category TEXT
      )
    `);
    console.log("[SUCCESS] Created 'products' table");

    // Insert sample data
    const products = [
      { id: 1, name: 'Laptop', price: 999.99, category: 'Electronics' },
      { id: 2, name: 'Coffee Mug', price: 12.99, category: 'Kitchen' },
      { id: 3, name: 'Wireless Mouse', price: 29.99, category: 'Electronics' },
      { id: 4, name: 'Notebook', price: 5.99, category: 'Office' },
      { id: 5, name: 'Smartphone', price: 699.99, category: 'Electronics' }
    ];

    for (const product of products) {
      db.exec(
        `INSERT INTO products VALUES (${product.id}, '${product.name}', ${product.price}, '${product.category}')`
      );
    }
    console.log(`[SUCCESS] Inserted ${products.length} products`);

    // Query all products
    console.log('\n[PRODUCTS] All products:');
    const allProducts = db.query('SELECT * FROM products');
    allProducts.forEach(row => {
      console.log(`   ID: ${row.id}, Name: ${row.name}, Price: $${row.price}, Category: ${row.category}`);
    });

    // Query electronics products
    console.log('\n[ELECTRONICS] Electronics products:');
    const electronics = db.query("SELECT name, price FROM products WHERE category = 'Electronics'");
    electronics.forEach(row => {
      console.log(`   ${row.name}: $${row.price}`);
    });

    // Query expensive products
    console.log('\n[EXPENSIVE] Expensive products (price > $100):');
    const expensiveProducts = db.query('SELECT name, price FROM products WHERE price > 100');
    expensiveProducts.forEach(row => {
      console.log(`   ${row.name}: $${row.price}`);
    });

    console.log(`\n[SUCCESS] Found ${expensiveProducts.length} expensive products`);

    // Clean up
    db.close();
    console.log('[SUCCESS] Original SQL API example completed successfully!\n');
    return true;

  } catch (error) {
    if (error instanceof RistrettoError) {
      console.error(`[ERROR] Database error: ${error.message}`);
    } else {
      console.error(`[ERROR] Unexpected error: ${error.message}`);
    }
    return false;
  }
}

function tableV2ApiExample() {
  console.log('[FAST] Table V2 Ultra-Fast API Example');
  console.log('=' .repeat(40));

  try {
    // Create ultra-fast table for real-time analytics
    const schema = `
      CREATE TABLE analytics_events (
        timestamp INTEGER,
        user_id INTEGER,
        event_type TEXT(16),
        page_url TEXT(64),
        duration_ms INTEGER
      )
    `;

    const table = RistrettoTable.create('analytics_data', schema);
    console.log('[SUCCESS] Created ultra-fast analytics table');
    console.log('   Optimized for 4.6M+ rows/second throughput');

    // Simulate real-time web analytics data ingestion
    console.log('\n[ANALYTICS] Simulating real-time analytics ingestion...');

    const eventTypes = ['page_view', 'click', 'scroll', 'form_submit', 'download'];
    const pages = ['/home', '/products', '/about', '/contact', '/checkout'];
    const baseTimestamp = Date.now();

    let successfulInserts = 0;
    const totalInserts = 50; // Simulate 50 analytics events

    for (let i = 0; i < totalInserts; i++) {
      try {
        // Create analytics event values
        const values = [
          RistrettoValue.integer(baseTimestamp + i * 1000), // Every second
          RistrettoValue.integer(Math.floor(Math.random() * 1000)), // Random user ID
          RistrettoValue.text(eventTypes[i % eventTypes.length]), // Event type
          RistrettoValue.text(pages[i % pages.length]), // Page URL
          RistrettoValue.integer(Math.floor(Math.random() * 5000) + 100) // Duration 100-5100ms
        ];

        // High-speed append (optimized for performance)
        if (table.appendRow(values)) {
          successfulInserts++;
        }

      } catch (error) {
        console.error(`   [WARNING] Failed to insert event ${i}: ${error.message}`);
      }
    }

    console.log('[SUCCESS] High-speed ingestion completed');
    console.log(`   Successfully processed: ${successfulInserts}/${totalInserts} events`);
    console.log(`   Total rows in table: ${table.getRowCount()}`);

    // Performance summary
    console.log('\n[PERFORMANCE] Performance Summary:');
    console.log(`   - Event processing rate: ~${totalInserts} events simulated`);
    console.log('   - Memory efficient: Fixed-width row format');
    console.log('   - Zero-copy I/O: Memory-mapped file access');
    console.log('   - Append-only: Optimized for write-heavy analytics workloads');

    // Real-world analytics insights
    console.log('\n[USE CASES] Real-World Analytics Use Cases:');
    console.log('   - User behavior tracking');
    console.log('   - Performance monitoring');
    console.log('   - A/B testing data collection');
    console.log('   - Conversion funnel analysis');
    console.log('   - Real-time dashboard feeds');

    if (table.getRowCount() !== successfulInserts) {
      throw new Error('row count does not match inserts');
    }

    // Read the rows back out to confirm the V2 scan decodes real values.
    console.log('\n[READBACK] First 3 rows read back from the table:');
    const scanned = table.select(null);
    scanned.slice(0, 3).forEach((cols, i) => {
      const [timestamp, userId, eventType, pageUrl, durationMs] = cols;
      console.log(
        `   Row ${i + 1}: timestamp=${timestamp}, user_id=${userId}, ` +
        `event_type=${eventType}, page_url=${pageUrl}, duration_ms=${durationMs}`
      );
    });
    if (scanned.length !== successfulInserts) {
      throw new Error(`scan returned ${scanned.length} rows, expected ${successfulInserts}`);
    }
    if (scanned.some(cols => cols.some(v => v === undefined))) {
      throw new Error('scan returned undefined field values');
    }
    console.log(`[SUCCESS] Scanned ${scanned.length} rows, all fields decoded`);

    // Clean up
    table.close();

    // Reopen and confirm the rows persisted across close/open.
    const reopened = RistrettoTable.open('analytics_data');
    if (reopened.getRowCount() !== successfulInserts) {
      reopened.close();
      throw new Error(`after reopen expected ${successfulInserts} rows, got ${reopened.getRowCount()}`);
    }
    console.log(`[SUCCESS] Reopened table, ${reopened.getRowCount()} rows persisted`);
    reopened.close();

    console.log('[SUCCESS] Table V2 ultra-fast API example completed successfully!\n');
    return true;

  } catch (error) {
    if (error instanceof RistrettoError) {
      console.error(`[ERROR] Table error: ${error.message}`);
    } else {
      console.error(`[ERROR] Unexpected error: ${error.message}`);
    }
    return false;
  }
}

function integrationExamples() {
  console.log('[INTEGRATION] Integration Examples');
  console.log('=' .repeat(30));

  console.log('Perfect use cases for RistrettoDB Node.js bindings:\n');

  const examples = [
    ['Web Applications', 'Express.js APIs, session storage, user analytics'],
    ['Real-time Analytics', 'Live dashboards, event tracking, metrics collection'],
    ['IoT Data Processing', 'Sensor data ingestion, device telemetry'],
    ['Gaming Backends', 'Player statistics, match data, leaderboards'],
    ['Security Logging', 'Audit trails, access logs, intrusion detection'],
    ['FinTech Applications', 'Transaction logging, trading data, compliance'],
    ['Mobile Backends', 'User data sync, offline-first applications'],
    ['Serverless Functions', 'AWS Lambda, Vercel, Netlify edge functions']
  ];

  examples.forEach(([category, description]) => {
    console.log(`   ${category}`);
    console.log(`     -> ${description}`);
  });

  console.log('\n[INSTALLATION] Installation:');
  console.log('   1. Build RistrettoDB: cd ../../ && make libraries');
  console.log('   2. Install dependencies: npm install');
  console.log('   3. Copy bindings to your project');
  console.log("   4. const { RistrettoDB } = require('./ristretto');");

  console.log('\n[PERFORMANCE] Performance Benefits:');
  console.log('   - 2.8x faster than SQLite (Original API)');
  console.log('   - 4.57x faster than SQLite (Table V2 API)');
  console.log('   - Async/await compatible');
  console.log('   - Single dependency: koffi (prebuilt FFI, no native build step)');
  console.log('   - Memory-efficient design');
  console.log('   - Perfect for microservices');

  console.log('\n[NODE.JS] Node.js Specific Advantages:');
  console.log('   - Non-blocking I/O friendly');
  console.log('   - Easy JSON data handling');
  console.log('   - Perfect for REST APIs');
  console.log('   - Excellent for real-time applications');
  console.log('   - Minimal memory footprint');
}

async function main() {
  console.log('[ACTIVE] RistrettoDB Node.js Bindings Example');
  console.log('=' .repeat(50));
  console.log('A tiny, blazingly fast, embeddable SQL engine');
  console.log('https://github.com/MonkeyIsNull/RistrettoDB');
  console.log();

  // Check if library is available
  try {
    const version = RistrettoDB.version();
    console.log(`[SUCCESS] RistrettoDB v${version} loaded successfully`);
    console.log();
  } catch (error) {
    console.error(`[ERROR] Failed to load RistrettoDB library: ${error.message}`);
    console.log('\n[INFO] Make sure to build the library first:');
    console.log('   cd ../../ && make libraries');
    console.log('\n[INFO] Install Node.js dependencies:');
    console.log('   npm install');
    return 1;
  }

  // Run examples
  let success = true;

  success = originalSqlApiExample() && success;
  success = tableV2ApiExample() && success;

  integrationExamples();

  console.log('\n' + '=' .repeat(50));
  if (success) {
    console.log('[SUCCESS] All examples completed successfully!');
    console.log('   Ready to integrate RistrettoDB into your Node.js applications!');
    console.log('');
    console.log('[NEXT STEPS] Next Steps:');
    console.log('   - Copy ristretto.js to your project');
    console.log('   - Install dependencies: npm install');
    console.log('   - Start building high-performance applications!');
  } else {
    console.log('[WARNING] Some examples failed. Check error messages above.');
    return 1;
  }

  return 0;
}

// Run the example
if (require.main === module) {
  main().then(exitCode => {
    process.exit(exitCode);
  }).catch(error => {
    console.error('[ERROR] Fatal error:', error);
    process.exit(1);
  });
}