#!/usr/bin/env bash
# Valgrind leak check for unit + a short stress run (doubles f128).
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

# Build only (do not run phony test-stress-f128, which executes the binary).
make buddy_doubles_f128 test_unit_api LEAF_FLOAT_TYPE=3 -j"${NPROC}"
rm -f test_stress_f128
make ./test_stress_f128 STRESS_LEVEL=1 LEAF_FLOAT_TYPE=3 -j"${NPROC}"

VG=(valgrind --leak-check=full --show-leak-kinds=all
    --errors-for-leak-kinds=definite,indirect,possible,reachable
    --error-exitcode=42
    --quiet)

echo "=== valgrind test_unit_api ==="
if "${VG[@]}" ./test_unit_api; then
    summary_record "valgrind test_unit_api" 0
else
    summary_record "valgrind test_unit_api" 1
fi

echo "=== valgrind test_stress_f128 (LEVEL=1 binary) ==="
if "${VG[@]}" ./test_stress_f128; then
    summary_record "valgrind test_stress_f128" 0
else
    summary_record "valgrind test_stress_f128" 1
fi

QASM="${ROOT}/benchmarks/no-measure/LP-Grover/05.qasm"
if [[ -f "${QASM}" ]]; then
    echo "=== valgrind MEDUSA symbolic LP-Grover/05 ==="
    if "${VG[@]}" ./MEDUSA_buddy_doubles_f128 --file "${QASM}" --symbolic >/dev/null; then
        summary_record "valgrind symbolic Grover/05" 0
    else
        summary_record "valgrind symbolic Grover/05" 1
    fi
else
    echo "skip symbolic valgrind (missing ${QASM})"
    summary_record "valgrind symbolic Grover/05" 0
fi

summary_print "test_leaks"
exit $?
