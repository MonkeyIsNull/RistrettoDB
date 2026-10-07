#!/usr/bin/env bash
#
# Compile and run every C embedding example against the built static library.
# Asserts each links and exits 0. Reused by CI.
#
# Prereq: `make static` (produces lib/libristretto.a).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

CC="${CC:-clang}"
CFLAGS="-O3 -std=c11 -Iinclude -Iembed -I."
LIB="lib/libristretto.a"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

if [ ! -f "$LIB" ]; then
    echo "ERROR: $LIB not found; run 'make static' first." >&2
    exit 1
fi

# name:extra-cflags
examples=(
    "basic_example.c:"
    "raw_api_demo.c:"
    "simple_embed.c:"
    "simple_embed_compat.c:"
    "embedding_demo.c:"
    "working_demo.c:"
    "direct_api_demo.c:-DRISTRETTO_NO_COMPATIBILITY_LAYER"
)

fail=0
for entry in "${examples[@]}"; do
    src="examples/${entry%%:*}"
    extra="${entry#*:}"
    bin="$OUT/$(basename "${src%.c}")"
    printf '  %-26s ' "$(basename "$src")"
    # shellcheck disable=SC2086
    if ! $CC $CFLAGS $extra -o "$bin" "$src" "$LIB" 2> "$OUT/err.log"; then
        echo "LINK FAIL"; cat "$OUT/err.log"; fail=1; continue
    fi
    if "$bin" > "$OUT/run.log" 2>&1; then
        echo "OK"
    else
        echo "RUN FAIL (exit $?)"; tail -5 "$OUT/run.log"; fail=1
    fi
done

if [ "$fail" -ne 0 ]; then
    echo "One or more C examples failed." >&2
    exit 1
fi
echo "All 7 C examples linked and ran."
