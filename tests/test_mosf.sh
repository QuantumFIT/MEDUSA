#!/usr/bin/env bash
# MOSF: the JSON input path (--tree-simulation, sim_mosf_file, src/sim_mosf.cpp).
#
# This file is compiled only under USE_CXX=1 and, until this suite, was never
# executed by anything: 0 of 104 lines, the largest wholly-uncovered file in
# the tree. It was also invisible in the default coverage report, since the C
# build does not compile the translation unit at all - so the headline number
# never accounted for it.
#
# The oracle is OpenQASM. Each NAME.mosf has a NAME.qasm describing the same
# circuit, and the two res.dot files must be identical. That works because the
# OpenQASM path is independently covered by every other suite, so a
# disagreement is a statement about the MOSF path rather than about both.
#
# See tests/mosf/README.md for the gate spellings and why the .mosf files
# shipped in lib/MoToBuddy cannot be used here.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=test_summary.sh
source "$(dirname "$0")/test_summary.sh"
summary_init

BIN="${MEDUSA_BIN:-${ROOT}/MEDUSA_buddy_doubles_f128}"
FIX="${ROOT}/tests/mosf"
WORKDIR="${ROOT}/.test-work/mosf_$$"
mkdir -p "${WORKDIR}"
trap 'rm -rf "${WORKDIR}"' EXIT

if [[ ! -x "${BIN}" ]]; then
    echo "Binary ${BIN} not found - build with: make buddy_doubles USE_CXX=1 LEAF_FLOAT_TYPE=3"
    exit 1
fi

# MOSF support is a compile-time option. Without it main.c prints a specific
# message and exits 1, so probe for that rather than guessing from the build.
probe="$( (cd "${WORKDIR}" && "${BIN}" --tree-simulation --file "${FIX}/h1.mosf" 2>&1) )"
if grep -q "MOSF support not compiled in" <<<"${probe}"; then
    echo "SKIP test_mosf: binary built without MOSF (rebuild with USE_CXX=1)"
    exit 0
fi

echo "MEDUSA MOSF (JSON input)"
echo "  bin=${BIN}"

# ---------------------------------------------------------------------------
# Equivalence: the same circuit as MOSF and as OpenQASM must agree exactly.
# ---------------------------------------------------------------------------
compare_to_qasm() {
    local name="$1"
    local mosf="${FIX}/${name}.mosf"
    local qasm="${FIX}/${name}.qasm"
    local label="mosf-vs-qasm-${name}"

    if [[ ! -f "${mosf}" || ! -f "${qasm}" ]]; then
        echo "FAIL ${label}: missing fixture pair"
        summary_record "${label}" 1
        return
    fi

    local rc=0
    ( cd "${WORKDIR}" && timeout 120 "${BIN}" --tree-simulation --file "${mosf}" \
        >"${WORKDIR}/${name}.mosf.log" 2>&1 ) || rc=$?
    if [[ "${rc}" -ne 0 ]]; then
        echo "FAIL ${label}: MOSF run exited ${rc}"
        tail -3 "${WORKDIR}/${name}.mosf.log" || true
        summary_record "${label}" 1
        return
    fi
    # Since issue #13 a failed run exits non-zero, so the check above already
    # catches most failures. This stays as a second line of defence: an error
    # reported on stderr must never coexist with a success exit.
    if grep -qi "^Error simulating MOSF" "${WORKDIR}/${name}.mosf.log"; then
        echo "FAIL ${label}: MOSF run reported an error"
        tail -3 "${WORKDIR}/${name}.mosf.log" || true
        summary_record "${label}" 1
        return
    fi
    cp -f "${WORKDIR}/res.dot" "${WORKDIR}/${name}.mosf.dot" 2>/dev/null || true

    ( cd "${WORKDIR}" && timeout 120 "${BIN}" --file "${qasm}" \
        >"${WORKDIR}/${name}.qasm.log" 2>&1 ) || rc=$?
    if [[ "${rc}" -ne 0 ]]; then
        echo "FAIL ${label}: OpenQASM run exited ${rc}"
        summary_record "${label}" 1
        return
    fi
    cp -f "${WORKDIR}/res.dot" "${WORKDIR}/${name}.qasm.dot" 2>/dev/null || true

    if [[ ! -s "${WORKDIR}/${name}.mosf.dot" || ! -s "${WORKDIR}/${name}.qasm.dot" ]]; then
        echo "FAIL ${label}: one of the runs produced no res.dot"
        summary_record "${label}" 1
        return
    fi

    # Compare basis-state amplitudes, not the files. Node ids in res.dot are
    # allocation order, so two runs that agree on every amplitude can still
    # differ byte for byte - which is exactly what the x-then-h fixtures do,
    # with their two terminals numbered the other way round. An earlier
    # version of this suite used cmp and passed only because the simpler
    # fixtures happened to allocate in the same order.
    local nq
    nq="$(sed -nE 's/.*qubit\[([0-9]+)\].*/\1/p' "${qasm}" | head -1)"
    if [[ -z "${nq}" ]]; then
        echo "FAIL ${label}: cannot read the qubit count from ${qasm##*/}"
        summary_record "${label}" 1
        return
    fi

    if python3 "${ROOT}/tests/dot_amps_equal.py" \
           "${WORKDIR}/${name}.mosf.dot" "${WORKDIR}/${name}.qasm.dot" "${nq}"; then
        echo "OK   ${label}"
        summary_record "${label}" 0
    else
        echo "FAIL ${label}: MOSF and OpenQASM disagree on a basis amplitude"
        summary_record "${label}" 1
    fi
}

# H applied to |0>: the high child is the zero BDD, so (low + high) and
# (low - high) coincide. These three therefore do NOT discriminate between
# plus_mulsqrt2 and minus_mulsqrt2 - mutation-checked, a build with the two
# swapped passes all three. They are kept because they still cover parse,
# traverse_to, makenode, swap and neg, but they are not the interesting ones.
compare_to_qasm h1     # H on one qubit
compare_to_qasm hh     # H on both levels of a two-qubit register
compare_to_qasm hxz    # H, X, Z - distinct +/-0.707 terminals

# X first, so H acts on |1> and the high child is non-zero. Now (low + high)
# and (low - high) differ, and the sign of every amplitude depends on the two
# binary ops being the right way round. These are the fixtures that actually
# constrain the arithmetic: swapping addLeafS and subLeafS in either registry
# entry fails both.
compare_to_qasm xh     # X then H on one qubit -> (|0> - |1>)/sqrt2
compare_to_qasm hxh    # the same on both levels of a two-qubit register

# The remaining registry entries. X first for the same reason as above: these
# multiply the high child, which is the zero BDD until something populates it.
#
# The OpenQASM side spells Sdg as S^3 deliberately - this parser has t/tdg but
# only s, no sdg, so `sdg` is rejected as an unknown command. An earlier draft
# used it and compared against an empty res.dot.
compare_to_qasm xs     # i_mul      == S
compare_to_qasm xsdg   # neg_i_mul  == S^3 == Sdg

# ---------------------------------------------------------------------------
# Failure handling: every one of these must be reported, and none may crash.
# ---------------------------------------------------------------------------
expect_error() {
    local name="$1" want="$2"
    local label="mosf-error-${name}"
    local log="${WORKDIR}/${name}.err.log"
    local rc=0
    ( cd "${WORKDIR}" && timeout 120 "${BIN}" --tree-simulation --file "${FIX}/${name}.mosf" \
        >"${log}" 2>&1 ) || rc=$?

    if [[ "${rc}" -eq 124 ]]; then
        echo "FAIL ${label}: timed out"
        summary_record "${label}" 1
        return
    fi
    if [[ "${rc}" -eq 139 || "${rc}" -eq 134 ]]; then
        echo "FAIL ${label}: crashed (exit ${rc}) instead of reporting the problem"
        summary_record "${label}" 1
        return
    fi
    if ! grep -qi -- "${want}" "${log}"; then
        echo "FAIL ${label}: expected a diagnostic mentioning '${want}'"
        tail -3 "${log}" || true
        summary_record "${label}" 1
        return
    fi
    echo "OK   ${label}"
    summary_record "${label}" 0
}

# sim_mosf_file catches std::exception and returns false; these pin that each
# distinct failure is reported specifically rather than collapsing into one
# generic message, which is what makes a bad MOSF file diagnosable.
expect_error bad_no_x_levels      "x_levels"
expect_error bad_unknown_op       "Unknown NodeOp type"
expect_error bad_malformed        "parse error"
expect_error bad_rx_unimplemented "rx not implemented"

# Gate-definition files from the MoToBuddy examples are not circuits: no
# x_levels, and they use plus_s/minus_s which MEDUSA does not register.
# Pinned so that if MEDUSA ever grows a registry entry for them, this turns
# into a deliberate decision rather than a silent change.
GATE_DEF="${ROOT}/lib/MoToBuddy/examples/quantum_gates/h.mosf"
if [[ -f "${GATE_DEF}" ]]; then
    label="mosf-error-vendored-gate-def"
    log="${WORKDIR}/gatedef.log"
    rc=0
    ( cd "${WORKDIR}" && timeout 120 "${BIN}" --tree-simulation --file "${GATE_DEF}" \
        >"${log}" 2>&1 ) || rc=$?
    if [[ "${rc}" -eq 139 || "${rc}" -eq 134 || "${rc}" -eq 124 ]]; then
        echo "FAIL ${label}: crashed or hung on a gate-definition file"
        summary_record "${label}" 1
    elif grep -qi "x_levels" "${log}"; then
        echo "OK   ${label}"
        summary_record "${label}" 0
    else
        echo "FAIL ${label}: expected it to be rejected for the missing x_levels"
        tail -3 "${log}" || true
        summary_record "${label}" 1
    fi
fi

# ---------------------------------------------------------------------------
# sim_mosf_file returns false on every error above. main() used to ignore that
# and still exit 0, so a failed MOSF run could not be told from a successful
# one by exit status - only by the message on stderr. Fixed with issue #13;
# this pins the exit status so it cannot regress to silence.
# ---------------------------------------------------------------------------
label="mosf-failure-exits-nonzero"
rc=0
( cd "${WORKDIR}" && timeout 120 "${BIN}" --tree-simulation --file "${FIX}/bad_unknown_op.mosf" \
    >/dev/null 2>&1 ) || rc=$?
if [[ "${rc}" -ne 0 && "${rc}" -ne 124 && "${rc}" -ne 139 && "${rc}" -ne 134 ]]; then
    echo "OK   ${label}"
    summary_record "${label}" 0
else
    echo "FAIL ${label}: expected a clean non-zero exit, got ${rc}"
    summary_record "${label}" 1
fi

summary_print "test_mosf"
exit $?
