#!/usr/bin/env bash
# Symbolic (--symbolic) sweep over the loop-program benchmarks, LP-*.
#
# The LP families are loop programs with up to 2^36 iterations; they are the
# workload the symbolic mode exists for, and nothing else in CI runs them.
# Every circuit runs under a wall-clock cap and an address-space cap, so a
# blow-up in the symbolic path cannot hang or thrash the runner.
#
# Verdicts per circuit:
#   ok    - finished in time with a valid res.dot
#   slow  - hit the cap, and the baseline says that is expected today
#   NEW   - finished although the baseline expected a timeout (progress:
#           add it to tests/lp_symbolic_expected.txt)
#   FAIL  - crashed, produced no digraph, or timed out on a circuit the
#           baseline says must finish (a symbolic-path regression)
#
# The baseline, tests/lp_symbolic_expected.txt, lists the circuits that finish
# comfortably inside the cap; it is the regression guard.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${ROOT}"
# shellcheck source=test_summary.sh
source "${ROOT}/tests/test_summary.sh"
summary_init

BIN="${MEDUSA_BIN:-${ROOT}/MEDUSA_buddy_doubles_f128}"
if [[ "${BIN}" != /* ]]; then
    BIN="${ROOT}/${BIN#./}"
fi
if [[ ! -x "${BIN}" ]]; then
    echo "Binary ${BIN} not found - build with: make buddy_doubles_f128"
    exit 1
fi

# Per-circuit wall-clock cap in seconds and address-space cap in KB.
TIMEOUT_SEC="${MEDUSA_TEST_TIMEOUT:-10}"
MEM_KB="${MEDUSA_TEST_MEM_KB:-2097152}"
BENCH_DIR="${ROOT}/benchmarks/no-measure"
EXPECTED="${ROOT}/tests/lp_symbolic_expected.txt"
FAMILIES="${LP_FAMILIES:-LP-Grover LP-PeriodFinding LP-QuantumCounting}"

WORKDIR="${ROOT}/.test-work/lp_symb_$$"
mkdir -p "${WORKDIR}"
trap 'rm -rf "${WORKDIR}"' EXIT
TABLE="${WORKDIR}/table.txt"

expected_to_finish() {
    grep -qxF -- "$1" "${EXPECTED}" 2>/dev/null
}

is_valid_dot() {
    [[ -s "$1" ]] && head -1 "$1" | grep -q 'digraph' && grep -q 'shape=box' "$1"
}

echo "MEDUSA symbolic LP sweep (${BIN}), cap ${TIMEOUT_SEC}s / ${MEM_KB} KB"
printf '%-40s %7s  %s\n' circuit time verdict | tee "${TABLE}"

for fam in ${FAMILIES}; do
    fam_fail=0
    for file in "${BENCH_DIR}/${fam}"/*.qasm; do
        rel="${file#"${BENCH_DIR}"/}"
        start=$(date +%s.%N)
        rc=0
        (
            cd "${WORKDIR}"
            rm -f res.dot
            ulimit -v "${MEM_KB}"
            timeout --signal=TERM "${TIMEOUT_SEC}" "${BIN}" --symbolic --file "${file}" \
                >run.log 2>&1
        ) || rc=$?
        elapsed=$(awk -v s="${start}" -v e="$(date +%s.%N)" 'BEGIN { printf "%.2f", e - s }')

        if [[ "${rc}" -eq 124 ]]; then
            if expected_to_finish "${rel}"; then
                verdict="FAIL (timeout, expected to finish)"; fam_fail=1
            else
                verdict="slow"
            fi
        elif [[ "${rc}" -ne 0 ]]; then
            verdict="FAIL (exit ${rc})"; fam_fail=1
            tail -5 "${WORKDIR}/run.log" | sed 's/^/    /'
        elif ! is_valid_dot "${WORKDIR}/res.dot"; then
            verdict="FAIL (no digraph)"; fam_fail=1
        elif expected_to_finish "${rel}"; then
            verdict="ok"
        else
            verdict="NEW (finished; add to lp_symbolic_expected.txt)"
        fi
        printf '%-40s %6ss  %s\n' "${rel}" "${elapsed}" "${verdict}" | tee -a "${TABLE}"
    done
    summary_record "${fam} (symbolic)" "${fam_fail}"
done

echo
echo "ok: $(grep -c '  ok$' "${TABLE}" || true)   slow: $(grep -c '  slow$' "${TABLE}" || true)" \
     "  NEW: $(grep -c '  NEW ' "${TABLE}" || true)   FAIL: $(grep -c '  FAIL' "${TABLE}" || true)"
summary_print "lp_symbolic"
exit $?
