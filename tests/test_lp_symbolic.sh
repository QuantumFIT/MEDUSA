#!/usr/bin/env bash
# Symbolic (--symbolic) sweep over the loop-program benchmarks, LP-*.
#
# The LP families are loop programs with up to 2^36 iterations; they are the
# workload the symbolic mode exists for, and nothing else in CI runs them.
# Every circuit runs under a wall-clock cap and an address-space cap, so a
# blow-up in the symbolic path cannot hang or thrash the runner.
#
# Correctness: a circuit that finishes symbolically is also run concretely under
# the same caps. When both finish, lp_dot_equiv.py must find the two res.dot
# files equivalent as state vectors (every amplitude within EQUIV_TOL) and of
# unit norm; when only the symbolic run finishes, its norm alone is checked.
# The symbolic and concrete paths share no gate code, so agreement between
# them is a real check on src/symb_utils.c and src/gates_symb.c.
#
# Verdicts per circuit:
#   ok    - finished in time with a valid res.dot; "=concrete" when the two
#           runs were compared, "norm" when only the symbolic norm was checked
#   slow  - hit the cap, and the baseline says that is expected today
#   NEW   - finished although the baseline expected a timeout (progress:
#           add it to tests/lp_symbolic_expected.txt)
#   FAIL  - crashed, produced no digraph, timed out on a circuit the baseline
#           says must finish (a symbolic-path regression), or disagrees with
#           the concrete run / has a non-unit norm (a wrong answer)
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
# Absolute tolerance on amplitudes and on |norm - 1| for the symbolic-vs-concrete
# comparison. The f128 runs agree to 1e-12 on every circuit that finishes both
# ways; 1e-9 leaves room for f64 builds via MEDUSA_BIN.
EQUIV_TOL="${MEDUSA_EQUIV_TOL:-1e-9}"
EQUIV="${ROOT}/tests/lp_dot_equiv.py"
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

qubit_count() {
    grep -oE 'qubit\[[0-9]+\]|qreg [A-Za-z_]+\[[0-9]+\]' "$1" | head -1 | grep -oE '[0-9]+'
}

# run_capped MODE FILE -> exit code; leaves the digraph in ${WORKDIR}/MODE.dot
run_capped() {
    local mode="$1" file="$2" rc=0
    (
        cd "${WORKDIR}"
        rm -f res.dot
        ulimit -v "${MEM_KB}"
        if [[ "${mode}" == symbolic ]]; then
            timeout --signal=TERM "${TIMEOUT_SEC}" "${BIN}" --symbolic --file "${file}" >run.log 2>&1
        else
            timeout --signal=TERM "${TIMEOUT_SEC}" "${BIN}" --file "${file}" >run.log 2>&1
        fi
    ) || rc=$?
    mv -f "${WORKDIR}/res.dot" "${WORKDIR}/${mode}.dot" 2>/dev/null || true
    return "${rc}"
}

echo "MEDUSA symbolic LP sweep (${BIN}), cap ${TIMEOUT_SEC}s / ${MEM_KB} KB"
printf '%-40s %7s  %s\n' circuit time verdict | tee "${TABLE}"

for fam in ${FAMILIES}; do
    fam_fail=0
    for file in "${BENCH_DIR}/${fam}"/*.qasm; do
        rel="${file#"${BENCH_DIR}"/}"
        start=$(date +%s.%N)
        rc=0
        run_capped symbolic "${file}" || rc=$?
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
        elif ! is_valid_dot "${WORKDIR}/symbolic.dot"; then
            verdict="FAIL (no digraph)"; fam_fail=1
        else
            # Correctness: against the concrete run when it finishes, else norm only.
            n="$(qubit_count "${file}")"
            crc=0
            run_capped concrete "${file}" || crc=$?
            if [[ "${crc}" -eq 0 ]] && is_valid_dot "${WORKDIR}/concrete.dot"; then
                other="${WORKDIR}/concrete.dot"; how="=concrete"
            else
                other="-"; how="norm"
            fi
            if ! eq_out="$(python3 "${EQUIV}" "${WORKDIR}/symbolic.dot" "${other}" "${n}" "${EQUIV_TOL}" 2>&1)"; then
                verdict="FAIL (${how}: ${eq_out})"; fam_fail=1
            elif expected_to_finish "${rel}"; then
                verdict="ok ${how}"
            else
                verdict="NEW ${how} (finished; add to lp_symbolic_expected.txt)"
            fi
        fi
        printf '%-40s %6ss  %s\n' "${rel}" "${elapsed}" "${verdict}" | tee -a "${TABLE}"
    done
    summary_record "${fam} (symbolic)" "${fam_fail}"
done

echo
echo "ok: $(grep -c '  ok ' "${TABLE}" || true) (=concrete: $(grep -c '  ok =concrete' "${TABLE}" || true))" \
     "  slow: $(grep -c '  slow$' "${TABLE}" || true)" \
     "  NEW: $(grep -c '  NEW ' "${TABLE}" || true)   FAIL: $(grep -c '  FAIL' "${TABLE}" || true)"
summary_print "lp_symbolic"
exit $?
