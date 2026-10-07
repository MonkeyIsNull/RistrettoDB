#!/usr/bin/env bash
#
# Compile and run every C embedding example against the built static library,
# plus the two amalgamation smoke tests. Asserts each links and exits 0.
# Reused by CI.
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

fail=0

# --- Linked C examples (Table V2) --------------------------------------------
# name:extra-cflags
examples=(
    "raw_api_demo.c:"
    "simple_embed.c:"
    "simple_embed_compat.c:"
    "embedding_demo.c:"
    "working_demo.c:"
    "direct_api_demo.c:-DRISTRETTO_NO_COMPATIBILITY_LAYER"
)

for entry in "${examples[@]}"; do
    src="examples/${entry%%:*}"
    extra="${entry#*:}"
    bin="$OUT/$(basename "${src%.c}")"
    printf '  %-26s ' "$(basename "$src")"
    # shellcheck disable=SC2086
    if ! $CC $CFLAGS $extra -o "$bin" "$src" "$LIB" 2> "$OUT/err.log"; then
        echo "LINK FAIL"; cat "$OUT/err.log"; fail=1; continue
    fi
    if ( cd "$OUT" && "$bin" > "$OUT/run.log" 2>&1 ); then
        echo "OK"
    else
        echo "RUN FAIL (exit $?)"; tail -5 "$OUT/run.log"; fail=1
    fi
done

# --- Amalgamation smoke tests ------------------------------------------------
# Option 2: ristretto.c compiled as a separate unit, then linked. The unit is
# compiled with -DRISTRETTO_EMBEDDED so it provides the full implementation;
# the consumer (test_embedded.c) includes ristretto.h for declarations.
printf '  %-26s ' "test_embedded.c"
if $CC $CFLAGS -DRISTRETTO_EMBEDDED -c embed/ristretto.c -o "$OUT/ristretto.o" 2> "$OUT/err.log" \
   && $CC $CFLAGS -o "$OUT/test_embedded" embed/test_embedded.c "$OUT/ristretto.o" 2>> "$OUT/err.log"; then
    if ( cd "$OUT" && "$OUT/test_embedded" > "$OUT/run.log" 2>&1 ); then echo "OK"; else echo "RUN FAIL"; tail -5 "$OUT/run.log"; fail=1; fi
else
    echo "LINK FAIL"; cat "$OUT/err.log"; fail=1
fi

# Option 1: single translation unit, #define RISTRETTO_EMBEDDED + #include .c
printf '  %-26s ' "test_embedded_compat.c"
if $CC $CFLAGS -o "$OUT/test_embedded_compat" embed/test_embedded_compat.c 2> "$OUT/err.log"; then
    if ( cd "$OUT" && "$OUT/test_embedded_compat" > "$OUT/run.log" 2>&1 ); then echo "OK"; else echo "RUN FAIL"; tail -5 "$OUT/run.log"; fail=1; fi
else
    echo "LINK FAIL"; cat "$OUT/err.log"; fail=1
fi

if [ "$fail" -ne 0 ]; then
    echo "One or more C examples failed." >&2
    exit 1
fi
echo "All 6 C examples + 2 amalgamation smoke tests linked and ran."
