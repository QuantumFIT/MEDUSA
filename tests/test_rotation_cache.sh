#!/usr/bin/env bash
# Regression: arbitrary-angle rotations must include the angle in the op-cache key.
#
# Catches both:
#   - MoToBuddy (int)param truncation (round doubles 0.0/0.5/1.0 collide)
#   - Sylvan syl_op cache omitting param entirely
#
# Uses MEDUSA_BIN (default MoToBuddy f128). make test-sylvan re-runs with Sylvan.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=test_summary.sh
source "$(dirname "$0")/test_summary.sh"
summary_init

QASM="${ROOT}/tests/qasm/rotation_cache"
BIN="${MEDUSA_BIN:-${ROOT}/MEDUSA_buddy_doubles_f128}"
if [[ "${BIN}" != /* ]]; then
    BIN="${ROOT}/${BIN#./}"
fi
WORKDIR="${ROOT}/.test-work/rotation_cache_$$"
mkdir -p "${WORKDIR}"
trap 'rm -rf "${WORKDIR}"' EXIT

if [[ ! -x "${BIN}" ]]; then
    echo "Binary ${BIN} not found — build with: make buddy_doubles_f128"
    echo "(or MEDUSA_BIN=./MEDUSA_sylvan_doubles_f128 after make sylvan_doubles)"
    exit 1
fi

echo "MEDUSA rotation-cache tests (${BIN})"

# Extract filled-box terminal amplitude labels (complex strings), sorted multiset.
# Buddy often emits the whole digraph on one line (invisible node + terminal), so
# match label= only inside attribute lists that also have shape=box + filled.
extract_amps() {
    python3 - "$1" <<'PY'
import re, sys
out = []
text = open(sys.argv[1], encoding="utf-8", errors="replace").read()
for attrs in re.findall(r"\[([^\]]*)\]", text):
    if "shape=box" not in attrs or "filled" not in attrs:
        continue
    m = re.search(r'label="([^"]*)"', attrs)
    if not m:
        continue
    s = m.group(1).strip().replace(" ", "")
    if not s or s in ("0", "F", "False", "false", "T", "True", "true", "NULL"):
        continue
    out.append(s)
print("\n".join(sorted(out)))
PY
}

run_circ() {
    local name="$1"
    local file="$2"
    local log="${WORKDIR}/${name}.log"
    local dot="${WORKDIR}/${name}.dot"
    if ! (
        cd "${WORKDIR}"
        "${BIN}" --file "${file}" >"${log}" 2>&1
        cp -f res.dot "${dot}"
    ); then
        echo "FAIL ${name}: simulator exited non-zero"
        tail -20 "${log}" || true
        summary_record "${name}" 1
        return 1
    fi
    if [[ ! -s "${dot}" ]]; then
        echo "FAIL ${name}: empty res.dot"
        summary_record "${name}" 1
        return 1
    fi
    extract_amps "${dot}" >"${WORKDIR}/${name}.amps"
    return 0
}

# Assert two amp multisets differ (cache must not drop the second angle).
assert_differ() {
    local label="$1"
    local a="$2"
    local b="$3"
    if [[ ! -f "${WORKDIR}/${a}.amps" || ! -f "${WORKDIR}/${b}.amps" ]]; then
        echo "FAIL ${label}: missing amp dumps"
        summary_record "${label}" 1
        return
    fi
    if [[ ! -s "${WORKDIR}/${a}.amps" || ! -s "${WORKDIR}/${b}.amps" ]]; then
        echo "FAIL ${label}: empty amp dump (res.dot parse failed?)"
        echo "  ${a}:"; sed 's/^/    /' "${WORKDIR}/${a}.amps" || true
        echo "  ${b}:"; sed 's/^/    /' "${WORKDIR}/${b}.amps" || true
        summary_record "${label}" 1
        return
    fi
    if cmp -s "${WORKDIR}/${a}.amps" "${WORKDIR}/${b}.amps"; then
        echo "FAIL ${label}: amplitudes identical (cache likely dropped an angle)"
        echo "  ${a}:"
        sed 's/^/    /' "${WORKDIR}/${a}.amps"
        echo "  ${b}:"
        sed 's/^/    /' "${WORKDIR}/${b}.amps"
        summary_record "${label}" 1
    else
        echo "OK   ${label}"
        summary_record "${label}" 0
    fi
}

# Assert two amp multisets match (rz(0) is identity; sequence ≡ single angle).
assert_same() {
    local label="$1"
    local a="$2"
    local b="$3"
    if [[ ! -f "${WORKDIR}/${a}.amps" || ! -f "${WORKDIR}/${b}.amps" ]]; then
        echo "FAIL ${label}: missing amp dumps"
        summary_record "${label}" 1
        return
    fi
    if [[ ! -s "${WORKDIR}/${a}.amps" || ! -s "${WORKDIR}/${b}.amps" ]]; then
        echo "FAIL ${label}: empty amp dump (res.dot parse failed?)"
        summary_record "${label}" 1
        return
    fi
    if cmp -s "${WORKDIR}/${a}.amps" "${WORKDIR}/${b}.amps"; then
        echo "OK   ${label}"
        summary_record "${label}" 0
    else
        echo "FAIL ${label}: amplitudes differ"
        echo "  ${a}:"
        sed 's/^/    /' "${WORKDIR}/${a}.amps"
        echo "  ${b}:"
        sed 's/^/    /' "${WORKDIR}/${b}.amps"
        summary_record "${label}" 1
    fi
}

# --- simulate fixtures ---
FIXTURES=(
    h
    h_rz0 h_rz1 h_rz0_rz1
    h_rz01 h_rz0_rz01
    h_rz05 h_rz05_rz1
    h_rx1 h_rx0_rx1
    h_ry1 h_ry0_ry1
    h_rz02 h_rz01_rz02
    hh hh_rz1q0 hh_rz0q0_rz1q0
    h_rz_pi4 h_rz_neg_pi8 h_rz_2pi3 h_rz_2pi3_num h_rz2
)

for f in "${FIXTURES[@]}"; do
    if ! run_circ "${f}" "${QASM}/${f}.qasm"; then
        : # already recorded
    fi
done

# Review blocker: h;rz(0);rz(1) must apply rz(1), not replay H / rz(0)
assert_differ "rz-round-vs-h"           h           h_rz0_rz1
assert_same   "rz-round-vs-rz1"         h_rz0_rz1   h_rz1
assert_same   "rz0-is-identity"         h           h_rz0

# Sylvan-style consecutive small angles
assert_differ "rz-0-then-0.1-vs-h"      h           h_rz0_rz01
assert_same   "rz-0-then-0.1-vs-0.1"    h_rz0_rz01  h_rz01

# Another round-double pair (0.5 and 1.0) — neither is identity
assert_differ "rz-0.5-then-1-vs-h"      h           h_rz05_rz1
assert_differ "rz-0.5-then-1-vs-0.5"    h_rz05      h_rz05_rz1
assert_differ "rz-0.5-then-1-vs-1"      h_rz1       h_rz05_rz1

# rx / ry round doubles
assert_differ "rx-round-vs-h"           h           h_rx0_rx1
assert_same   "rx-round-vs-rx1"         h_rx0_rx1   h_rx1
assert_differ "ry-round-vs-h"           h           h_ry0_ry1
assert_same   "ry-round-vs-ry1"         h_ry0_ry1   h_ry1

# Two non-identity angles must not collapse to the first alone
assert_differ "rz-0.1-then-0.2-vs-0.1"  h_rz01      h_rz01_rz02
assert_differ "rz-0.1-then-0.2-vs-0.2"  h_rz02      h_rz01_rz02

# Two-qubit diagram: angle still in key with H on both qubits
assert_differ "hh-rz-round-vs-hh"       hh          hh_rz0q0_rz1q0
assert_same   "hh-rz-round-vs-rz1"      hh_rz0q0_rz1q0 hh_rz1q0

# Blocker 3: OpenQASM pi-relative angles
assert_differ "rz-pi4-vs-h"             h           h_rz_pi4
assert_differ "rz-neg-pi8-vs-h"         h           h_rz_neg_pi8
assert_same   "rz-2pi3-vs-numeric"      h_rz_2pi3   h_rz_2pi3_num
assert_differ "rz-2pi3-vs-rz2"          h_rz_2pi3   h_rz2

# Blocker 4: arbitrary-angle rotations must error under --symbolic (no silent drop).
assert_symb_rejects() {
    local label="$1"
    local file="$2"
    local needle="$3"
    local log="${WORKDIR}/${label}.log"
    if (
        cd "${WORKDIR}"
        "${BIN}" --file "${file}" --symbolic >"${log}" 2>&1
    ); then
        echo "FAIL ${label}: expected non-zero exit under --symbolic"
        summary_record "${label}" 1
        return
    fi
    if ! grep -q "${needle}" "${log}"; then
        echo "FAIL ${label}: missing error text '${needle}'"
        sed 's/^/    /' "${log}" || true
        summary_record "${label}" 1
        return
    fi
    echo "OK   ${label}"
    summary_record "${label}" 0
}

assert_symb_rejects "symb-reject-rz" "${QASM}/h_rz1.qasm" "Arbitrary-angle rz is not supported with symbolic"
assert_symb_rejects "symb-reject-rx" "${QASM}/h_rx1.qasm" "Arbitrary-angle rx is not supported with symbolic"
assert_symb_rejects "symb-reject-ry" "${QASM}/h_ry1.qasm" "Arbitrary-angle ry is not supported with symbolic"

summary_print "test_rotation_cache"
exit $?
