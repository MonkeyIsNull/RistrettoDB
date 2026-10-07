# Security Policy

## Supported versions

RistrettoDB is early-stage software. Security fixes are applied to the latest
0.3.x release only.

| Version | Supported |
|---------|-----------|
| 0.3.x   | ✅        |
| < 0.3   | ❌        |

## Reporting a vulnerability

Please report suspected vulnerabilities privately rather than opening a public
issue. Use GitHub's **private security advisory** feature on the repository
(Security → Report a vulnerability) at
<https://github.com/MonkeyIsNull/RistrettoDB>.

Include a description, reproduction steps, and the affected version/commit.
You can expect an initial acknowledgement within a reasonable time; please allow
time for a fix before any public disclosure.

## Non-guarantees (by design)

RistrettoDB is a fixed-schema, append-only, single-writer embedded store. The
following are deliberate design limits, not vulnerabilities:

- **Single-writer only.** Concurrency is guarded by an advisory `flock`, which
  is advisory (cooperating processes only) and a no-op on some network
  filesystems (NFS/SMB). There is no multi-writer support.
- **No crash-durability / WAL.** Writes are flushed with `msync` (async during
  writes, synchronous on `table_flush_durable` and on close) plus `fsync` on
  close. There is no write-ahead log: rows written since the last durable flush
  can be lost on a crash or power failure.
- **Schema input is trusted.** `table_create` parses a `CREATE TABLE` schema
  string; it is intended for application-controlled schemas, not adversarial
  input. (A fuzz harness exercises the parser for memory safety.)
- **No encryption or access control.** Data files are plain on-disk bytes;
  protect them with filesystem permissions.
- **Platform scope.** Supported on POSIX, little-endian, 64-bit systems only.
