#!/usr/bin/env node
/**
 * RistrettoDB Node.js Example (Table V2)
 *
 * Shows how to use RistrettoDB from Node.js: create a fixed-schema table,
 * append rows (including a NULL), scan them back, and confirm they persist
 * across a close/reopen cycle.
 *
 * Requirements:
 *   - Node.js 18+
 *   - RistrettoDB library built (run: cd ../../ && make dynamic)
 *   - Dependencies installed (run: npm install)
 *
 * Usage:
 *   node example.js
 */

const { RistrettoTable, RistrettoValue, RistrettoError, version } = require('./ristretto');

function tableV2ApiExample() {
  console.log('[FAST] Table V2 API Example');
  console.log('='.repeat(40));

  try {
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
    console.log('[SUCCESS] Created analytics table');

    const eventTypes = ['page_view', 'click', 'scroll', 'form_submit', 'download'];
    const pages = ['/home', '/products', '/about', '/contact', '/checkout'];
    const baseTimestamp = Date.now();

    let successfulInserts = 0;
    const totalInserts = 50;

    for (let i = 0; i < totalInserts; i++) {
      // Every 10th event has an unknown (NULL) event_type to show NULLs persist.
      const eventType = i % 10 === 0
        ? RistrettoValue.null()
        : RistrettoValue.text(eventTypes[i % eventTypes.length]);
      const values = [
        RistrettoValue.integer(baseTimestamp + i * 1000),
        RistrettoValue.integer(Math.floor(Math.random() * 1000)),
        eventType,
        RistrettoValue.text(pages[i % pages.length]),
        RistrettoValue.integer(Math.floor(Math.random() * 5000) + 100),
      ];
      if (table.appendRow(values)) successfulInserts++;
    }

    console.log(`[SUCCESS] Inserted ${successfulInserts}/${totalInserts} events`);
    console.log(`   Total rows in table: ${table.getRowCount()}`);

    if (table.getRowCount() !== successfulInserts) {
      throw new Error('row count does not match inserts');
    }

    // Read the rows back out to confirm the V2 scan decodes real values.
    console.log('\n[READBACK] First 3 rows read back from the table:');
    const scanned = table.select();
    scanned.slice(0, 3).forEach((cols, i) => {
      const [timestamp, userId, eventType, pageUrl, durationMs] = cols;
      console.log(
        `   Row ${i + 1}: timestamp=${timestamp}, user_id=${userId}, ` +
        `event_type=${eventType === null ? 'NULL' : eventType}, ` +
        `page_url=${pageUrl}, duration_ms=${durationMs}`
      );
    });
    if (scanned.length !== successfulInserts) {
      throw new Error(`scan returned ${scanned.length} rows, expected ${successfulInserts}`);
    }
    if (scanned.some(cols => cols.some(v => v === undefined))) {
      throw new Error('scan returned undefined field values');
    }
    const nullRows = scanned.filter(cols => cols[2] === null);
    if (nullRows.length === 0) {
      throw new Error('expected at least one NULL event_type to round-trip as null');
    }
    console.log(`[SUCCESS] Scanned ${scanned.length} rows; ` +
      `${nullRows.length} have a NULL event_type (round-tripped as null)`);

    table.close();

    // Reopen and confirm the rows persisted across close/open.
    const reopened = RistrettoTable.open('analytics_data', 5);
    if (reopened.getRowCount() !== successfulInserts) {
      reopened.close();
      throw new Error(`after reopen expected ${successfulInserts} rows, got ${reopened.getRowCount()}`);
    }
    const reopenedRows = reopened.select();
    if (!reopenedRows.some(cols => cols[2] === null)) {
      reopened.close();
      throw new Error('NULL event_type did not survive reopen');
    }
    console.log(`[SUCCESS] Reopened table, ${reopened.getRowCount()} rows persisted (NULL preserved)`);
    reopened.close();

    console.log('[SUCCESS] Table V2 API example completed successfully!\n');
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

async function main() {
  console.log('[ACTIVE] RistrettoDB Node.js Bindings Example (Table V2)');
  console.log('='.repeat(50));
  console.log('A fast, embeddable, append-only telemetry/analytics store');
  console.log('https://github.com/MonkeyIsNull/RistrettoDB');
  console.log();

  try {
    console.log(`[SUCCESS] RistrettoDB v${version()} loaded successfully\n`);
  } catch (error) {
    console.error(`[ERROR] Failed to load RistrettoDB library: ${error.message}`);
    console.log('\n[INFO] Build the library first:  cd ../../ && make dynamic');
    console.log('[INFO] Install dependencies:     npm install');
    return 1;
  }

  const success = tableV2ApiExample();

  console.log('='.repeat(50));
  if (success) {
    console.log('[SUCCESS] Example completed successfully!');
    return 0;
  }
  console.log('[WARNING] Example failed. Check error messages above.');
  return 1;
}

if (require.main === module) {
  main().then(exitCode => {
    process.exit(exitCode);
  }).catch(error => {
    console.error('[ERROR] Fatal error:', error);
    process.exit(1);
  });
}
