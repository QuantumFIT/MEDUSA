#!/usr/bin/env bash
# Valgrind leak check for unit + a short stress run (doubles f64).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${ROOT}"
# shellcheck source=test_summary.sh
source "${ROOT}/tests/test_summary.sh"
summary_init

if ! command -v valgrind >/dev/null 2>&1; then
    echo "valgrind not installed"
    exit 1
fi

NPROC="$(nproc 2>/dev/null || echo 4)"

# Build only (do not run phony test-stress-f64, which executes the binary).
make buddy_doubles_f64 test_unit_api -j"${NPROC}"
rm -f test_stress_f64
make ./test_stress_f64 STRESS_LEVEL=1 -j"${NPROC}"

VG=(valgrind --leak-check=full --show-leak-kinds=definite,indirect
    --errors-for-leak-kinds=definite
    --error-exitcode=42
    --quiet)

echo "=== valgrind test_unit_api ==="
if "${VG[@]}" ./test_unit_api; then
    summary_record "valgrind test_unit_api" 0
else
    summary_record "valgrind test_unit_api" 1
fi

echo "=== valgrind test_stress_f64 (LEVEL=1 binary) ==="
if "${VG[@]}" ./test_stress_f64; then
    summary_record "valgrind test_stress_f64" 0
else
    summary_record "valgrind test_stress_f64" 1
fi

summary_print "test_leaks"
exit $?
