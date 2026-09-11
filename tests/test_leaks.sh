#!/usr/bin/env bash
# Valgrind leak check (doubles f128).
#
# Coverage note: the unit + short-stress + symbolic-Grover trio reaches 54% of
# the lines in this build, and misses most of the allocation-dense code -
# leaf_reim_double.c (the file with the most malloc/free in the tree) sat at
# 52%, and qparam.c at 0%. Adding the metamorphic and benchmark-semantics
# suites takes it to 73%: they are the workloads that actually churn memory,
# with terminal floods, wide product states forcing a customPointers realloc,
# GC hammering and repeated initPackage/freePackage cycles. That addition is
# what first caught the free_sim_info leak in the test helpers.
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
make ./test_benchmark_semantics ./test_metamorphic LEAF_FLOAT_TYPE=3 -j"${NPROC}"
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

# The allocation-heavy pair. test_metamorphic is the slow one (~38s under
# valgrind against 1.4s native, the usual ~66x), and it is the run that reaches
# the terminal table, the GC paths and the leaf arithmetic hardest.
echo "=== valgrind test_benchmark_semantics ==="
if "${VG[@]}" ./test_benchmark_semantics; then
    summary_record "valgrind test_benchmark_semantics" 0
else
    summary_record "valgrind test_benchmark_semantics" 1
fi

echo "=== valgrind test_metamorphic ==="
if "${VG[@]}" ./test_metamorphic; then
    summary_record "valgrind test_metamorphic" 0
else
    summary_record "valgrind test_metamorphic" 1
fi

summary_print "test_leaks"
exit $?
