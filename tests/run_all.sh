#!/usr/bin/env bash
set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOTAL_FAIL=0

shopt -s nullglob
for script in "$DIR"/at_*.sh; do
    echo "=== Running $(basename "$script") ==="
    if ! bash "$script"; then
        TOTAL_FAIL=$((TOTAL_FAIL + 1))
    fi
done

if (( TOTAL_FAIL > 0 )); then
    echo "OVERALL: $TOTAL_FAIL test suite(s) failed." >&2
    exit 1
fi
echo "OVERALL: ALL SUITES PASSED."
exit 0
