#!/usr/bin/env bash
# Validate representative benchmarks: exit 0, valid MTBDD digraph, state norm ~ 1.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${ROOT}"
# shellcheck source=test_summary.sh
source "${ROOT}/tests/test_summary.sh"
summary_init

# Default: preferred MoToBuddy f128 binary. Override with MEDUSA_BIN for Sylvan.
BIN="${MEDUSA_BIN:-${ROOT}/MEDUSA_buddy_doubles_f128}"
if [[ "${BIN}" != /* ]]; then
    BIN="${ROOT}/${BIN#./}"
fi
WORKDIR="${ROOT}/.test-work/bench_$$"
mkdir -p "${WORKDIR}"
trap 'rm -rf "${WORKDIR}"' EXIT
# Optional per-circuit timeout (set by make test-sylvan). Unset = no timeout.
TIMEOUT_SEC="${MEDUSA_TEST_TIMEOUT:-}"

run_timeout() {
    if [[ -n "${TIMEOUT_SEC}" ]] && command -v timeout >/dev/null 2>&1; then
        timeout --signal=TERM "${TIMEOUT_SEC}" "$@"
    else
        "$@"
    fi
}

# Max allowed |total_prob - 1| over the whole circuit (f128 accumulation).
MAX_NORM_DEV="${MAX_NORM_DEV:-1e-6}"

is_valid_dot() {
    local dot="$1"
    [[ -s "${dot}" ]] || return 1
    head -1 "${dot}" | grep -q 'digraph' || return 1
    # Must contain at least one terminal box (false and/or complex leaf).
    grep -q 'shape=box' "${dot}" || return 1
    return 0
}

norm_ok() {
    local csv="$1"
    local maxdev
    maxdev=$(awk -F: '/max_norm_dev/ {print $2}' "${csv}" | tail -1)
    [[ -n "${maxdev}" ]] || return 1
    awk -v d="${maxdev}" -v lim="${MAX_NORM_DEV}" 'BEGIN { exit !(d+0 <= lim+0) }'
}

run_one() {
    local label="$1"
    local file="$2"
    local extra="${3:-}"
    local csv="${WORKDIR}/${label}.csv"
    local log="${WORKDIR}/${label}.log"
    local dot="${WORKDIR}/${label}.dot"

    if [[ ! -f "${file}" ]]; then
        echo "FAIL ${label}: missing file ${file}"
        summary_record "${label}" 1
        return
    fi

    if ! (
        cd "${WORKDIR}"
        run_timeout "${BIN}" --file "${file}" --norm-error --norm-csv "${csv}" ${extra} \
            >"${log}" 2>&1
        cp -f res.dot "${dot}" 2>/dev/null || true
    ); then
        echo "FAIL ${label}: simulator exited non-zero"
        tail -30 "${log}" || true
        summary_record "${label}" 1
        return
    fi

    if ! is_valid_dot "${dot}"; then
        echo "FAIL ${label}: invalid/empty MTBDD digraph in res.dot"
        head -5 "${dot}" 2>/dev/null || true
        summary_record "${label}" 1
        return
    fi

    if [[ ! -s "${csv}" ]]; then
        echo "FAIL ${label}: missing norm CSV"
        summary_record "${label}" 1
        return
    fi

    if ! norm_ok "${csv}"; then
        local maxdev
        maxdev=$(awk -F: '/max_norm_dev/ {print $2}' "${csv}" | tail -1)
        echo "FAIL ${label}: max_norm_dev=${maxdev} exceeds ${MAX_NORM_DEV}"
        summary_record "${label}" 1
        return
    fi

    local maxdev leaves
    maxdev=$(awk -F: '/max_norm_dev/ {print $2}' "${csv}" | tail -1)
    leaves=$(grep -c 'shape=box' "${dot}" || true)
    echo "OK   ${label}  (max_norm_dev=${maxdev}, terminals~${leaves})"
    summary_record "${label}" 0
}

if [[ ! -x "${BIN}" ]]; then
    echo "Binary ${BIN} not found — build with: make buddy_doubles_f128"
    echo "(or MEDUSA_BIN=./MEDUSA_sylvan_doubles_f128 after make sylvan_doubles)"
    exit 1
fi

echo "MEDUSA benchmark algorithm MTBDD validity tests (${BIN})"
echo "(norm limit |p-1| <= ${MAX_NORM_DEV})"

run_one "BV-nm-01"    "${ROOT}/benchmarks/no-measure/BernsteinVazirani/01.qasm"
run_one "BV-nm-05"    "${ROOT}/benchmarks/no-measure/BernsteinVazirani/05.qasm"
run_one "BV-m-01"     "${ROOT}/benchmarks/measure/BernsteinVazirani/01.qasm"

run_one "MCToffoli-03" "${ROOT}/benchmarks/no-measure/MCToffoli/03.qasm"
run_one "MCToffoli-06" "${ROOT}/benchmarks/no-measure/MCToffoli/06.qasm"
run_one "MCToffoli-m-03" "${ROOT}/benchmarks/measure/MCToffoli/03.qasm"

run_one "Feynman-tof3"     "${ROOT}/benchmarks/no-measure/Feynman/tof_3.qasm"
run_one "Feynman-barenco3" "${ROOT}/benchmarks/no-measure/Feynman/barenco_tof_3.qasm"
run_one "Feynman-mod5"     "${ROOT}/benchmarks/no-measure/Feynman/mod5_4.qasm"

run_one "LP-Grover-05"    "${ROOT}/benchmarks/no-measure/LP-Grover/05.qasm"
if [[ -f "${ROOT}/benchmarks/no-measure/LP-Grover/NL_06.qasm" ]]; then
    run_one "LP-Grover-NL06" "${ROOT}/benchmarks/no-measure/LP-Grover/NL_06.qasm"
fi
run_one "MOGrover-03"     "${ROOT}/benchmarks/no-measure/MOGrover/03.qasm"

run_one "LP-PF-07" "${ROOT}/benchmarks/no-measure/LP-PeriodFinding/07_03_05_0.qasm"
run_one "LP-QC-07" "${ROOT}/benchmarks/no-measure/LP-QuantumCounting/07_03_05_0.qasm"

run_one "RevLib-peres" "${ROOT}/benchmarks/no-measure/RevLib/peres_9.qasm"
run_one "RevLib-4gt11" "${ROOT}/benchmarks/no-measure/RevLib/4gt11_84.qasm"
run_one "Random-03"    "${ROOT}/benchmarks/no-measure/Random/03.qasm"

summary_print "test_benchmarks"
exit $?
