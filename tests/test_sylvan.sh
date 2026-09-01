#!/usr/bin/env bash
# Sylvan-backend extras on top of test_circuits.sh / test_benchmarks.sh.
# MoToBuddy remains the preferred default product; this suite only checks that
# the optional Sylvan C backend still simulates real circuits.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=test_summary.sh
source "$(dirname "$0")/test_summary.sh"
summary_init

SYL_F128="${ROOT}/MEDUSA_sylvan_doubles_f128"
SYL_GMP="${ROOT}/MEDUSA_sylvan_gmp"
BUD_F128="${ROOT}/MEDUSA_buddy_doubles_f128"
TIMEOUT_SEC="${SYLVAN_TEST_TIMEOUT:-90}"
WORKDIR="${ROOT}/.test-work/sylvan_$$"
mkdir -p "${WORKDIR}"
trap 'rm -rf "${WORKDIR}"' EXIT

if [[ ! -x "${SYL_F128}" ]]; then
    echo "Binary ${SYL_F128} not found — build with: make init-sylvan && make sylvan_doubles"
    exit 1
fi

run_timeout() {
    local secs="$1"
    shift
    if command -v timeout >/dev/null 2>&1; then
        timeout --signal=TERM "${secs}" "$@"
    else
        "$@"
    fi
}

run_one() {
    local label="$1"
    local bin="$2"
    local file="$3"
    local extra="${4:-}"
    local log="${WORKDIR}/${label}.log"
    local dot="${WORKDIR}/${label}.dot"

    if [[ ! -f "${file}" ]]; then
        echo "FAIL ${label}: missing ${file}"
        summary_record "${label}" 1
        return
    fi

    if ! (
        cd "${WORKDIR}"
        # shellcheck disable=SC2086
        run_timeout "${TIMEOUT_SEC}" "${bin}" --file "${file}" --norm-error ${extra} \
            >"${log}" 2>&1
        cp -f res.dot "${dot}" 2>/dev/null || true
    ); then
        echo "FAIL ${label}: simulator exited non-zero or timed out (${TIMEOUT_SEC}s)"
        tail -20 "${log}" || true
        summary_record "${label}" 1
        return
    fi

    if [[ ! -s "${dot}" ]] || ! head -1 "${dot}" | grep -q 'digraph'; then
        echo "FAIL ${label}: missing/invalid res.dot"
        summary_record "${label}" 1
        return
    fi

    echo "OK   ${label}"
    summary_record "${label}" 0
}

# Spot-check that probability terminals are numeric and match Buddy within tol.
compare_prob() {
    local label="$1"
    local file="$2"
    local syl_dot="${WORKDIR}/${label}.syl.dot"
    local bud_dot="${WORKDIR}/${label}.bud.dot"
    local log="${WORKDIR}/${label}.cmp.log"

    if [[ ! -x "${BUD_F128}" ]]; then
        echo "SKIP ${label}: Buddy binary missing (compare needs make buddy_doubles_f128)"
        summary_record "${label}" 0
        return
    fi

    if ! (
        cd "${WORKDIR}"
        run_timeout "${TIMEOUT_SEC}" "${SYL_F128}" --file "${file}" --probability >"${log}" 2>&1
        cp -f res.dot "${syl_dot}"
        run_timeout "${TIMEOUT_SEC}" "${BUD_F128}" --file "${file}" --probability >>"${log}" 2>&1
        cp -f res.dot "${bud_dot}"
    ); then
        echo "FAIL ${label}: compare run failed"
        tail -20 "${log}" || true
        summary_record "${label}" 1
        return
    fi

    if python3 - "${syl_dot}" "${bud_dot}" <<'PY'
import re, sys

def nums(path):
    # Only filled box terminals (Sylvan: shape=box, style=filled, label=...;
    # Buddy: label=..., style=filled,shape=box). Skip false/true sentinels.
    out = []
    for line in open(path, encoding="utf-8", errors="replace"):
        if "shape=box" not in line or "filled" not in line:
            continue
        m = re.search(r'label="([^"]*)"', line)
        if not m:
            continue
        s = m.group(1).strip().replace(" ", "")
        if s in ("0", "F", "False", "false", "T", "True", "true", "NULL"):
            continue
        try:
            out.append(float(s))
        except ValueError:
            continue
    return sorted(out)

a, b = nums(sys.argv[1]), nums(sys.argv[2])
if not a or not b:
    sys.exit(2)
# Multisets of |amp|^2 (or amp) should match within a loose f128/print tol.
if len(a) != len(b):
    sys.exit(3)
for x, y in zip(a, b):
    scale = max(1.0, abs(x), abs(y))
    if abs(x - y) > 1e-6 * scale and abs(x - y) > 1e-9:
        sys.exit(4)
sys.exit(0)
PY
    then
        echo "OK   ${label}"
        summary_record "${label}" 0
    else
        echo "FAIL ${label}: Sylvan vs Buddy probability terminals differ"
        summary_record "${label}" 1
    fi
}

echo "MEDUSA Sylvan extras (preferred product is still MoToBuddy)"
echo "  f128=${SYL_F128}"
echo "  timeout=${TIMEOUT_SEC}s"

# Harder than the small circuit smokes: loops, many CCX, 10–14 qubits.
run_one "syl-Grover-05"     "${SYL_F128}" "${ROOT}/benchmarks/no-measure/LP-Grover/05.qasm"
run_one "syl-Grover-06"     "${SYL_F128}" "${ROOT}/benchmarks/no-measure/LP-Grover/06.qasm"
run_one "syl-Grover-07"     "${SYL_F128}" "${ROOT}/benchmarks/no-measure/LP-Grover/07.qasm"
run_one "syl-Grover-NL06"   "${SYL_F128}" "${ROOT}/benchmarks/no-measure/LP-Grover/NL_06.qasm"
run_one "syl-Grover-05-symb" "${SYL_F128}" "${ROOT}/benchmarks/no-measure/LP-Grover/05.qasm" "--symbolic"
run_one "syl-MCToffoli-12"  "${SYL_F128}" "${ROOT}/benchmarks/no-measure/MCToffoli/12.qasm"
run_one "syl-MCToffoli-16"  "${SYL_F128}" "${ROOT}/benchmarks/no-measure/MCToffoli/16.qasm"
run_one "syl-MOGrover-04"   "${SYL_F128}" "${ROOT}/benchmarks/no-measure/MOGrover/04.qasm"
run_one "syl-Feynman-barenco3" "${SYL_F128}" "${ROOT}/benchmarks/no-measure/Feynman/barenco_tof_3.qasm"
run_one "syl-Feynman-barenco4" "${SYL_F128}" "${ROOT}/benchmarks/no-measure/Feynman/barenco_tof_4.qasm"
run_one "syl-BV-05"         "${SYL_F128}" "${ROOT}/benchmarks/no-measure/BernsteinVazirani/05.qasm"
run_one "syl-LP-PF-07"      "${SYL_F128}" "${ROOT}/benchmarks/no-measure/LP-PeriodFinding/07_03_05_0.qasm"

compare_prob "syl-vs-buddy-hth"  "${ROOT}/tests/qasm/metamorphic/hth_unrolled.qasm"
compare_prob "syl-vs-buddy-h2"   "${ROOT}/tests/qasm/metamorphic/identity_h2.qasm"
compare_prob "syl-vs-buddy-cx2"  "${ROOT}/tests/qasm/metamorphic/identity_cx2.qasm"
compare_prob "syl-vs-buddy-bell" "${ROOT}/tests/qasm/metamorphic/bell_roundtrip.qasm"
compare_prob "syl-vs-buddy-cz"   "${ROOT}/tests/qasm/metamorphic/cz_c0_t1.qasm"

if [[ -x "${SYL_GMP}" ]]; then
    run_one "syl-gmp-h2" "${SYL_GMP}" "${ROOT}/tests/qasm/metamorphic/identity_h2.qasm"
    run_one "syl-gmp-Grover-05" "${SYL_GMP}" "${ROOT}/benchmarks/no-measure/LP-Grover/05.qasm"
else
    echo "SKIP syl-gmp (build with: make sylvan_gmp)"
fi

summary_print "test_sylvan"
exit $?
