#!/usr/bin/env bash
# Command line handling, parser diagnostics, and the long-number output file.
#
# These are the paths a user hits first and the suites reached last. Before
# this file main.c sat at 55.7% branch coverage and sim.c at 66.4%: every
# existing suite feeds the simulator a well-formed circuit and correct
# arguments, so the entire diagnostic half of both files was unexercised.
#
# What matters here is not that the program fails on bad input but that its
# behaviour is pinned: a refusal is a non-zero exit and a message naming the
# problem, never a crash or a hang.
#
# This suite originally pinned three lenient behaviours as known gaps rather
# than asserting what they ought to do, and issue #13 was filed from them.
# With #13 fixed the assertions are inverted: all three are now refusals, and
# the two parser cases check the diagnostic text as well, since being told
# which qubit is out of range is the point.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=test_summary.sh
source "$(dirname "$0")/test_summary.sh"
summary_init

BIN="${MEDUSA_BIN:-${ROOT}/MEDUSA_buddy_doubles_f128}"
GMP_BIN="${MEDUSA_GMP_BIN:-${ROOT}/MEDUSA_buddy_gmp}"
WORKDIR="${ROOT}/.test-work/cli_$$"
mkdir -p "${WORKDIR}"
trap 'rm -rf "${WORKDIR}"' EXIT

if [[ ! -x "${BIN}" ]]; then
    echo "Binary ${BIN} not found - build with: make buddy_doubles LEAF_FLOAT_TYPE=3"
    exit 1
fi

# Run the simulator in an isolated cwd (it writes res.dot next to itself) and
# check the exit status and, optionally, that stderr mentions something.
# expect_exit: a number, or "nonzero".
check() {
    local label="$1" expect_exit="$2" expect_msg="$3"
    shift 3
    local log="${WORKDIR}/${label}.log"
    local rc=0
    ( cd "${WORKDIR}" && timeout 60 "$@" >"${log}" 2>&1 ) || rc=$?

    if [[ "${expect_exit}" == "nonzero" ]]; then
        if [[ "${rc}" -eq 0 ]]; then
            echo "FAIL ${label}: expected a non-zero exit, got 0"
            summary_record "${label}" 1
            return
        fi
        # 124 is timeout(1): a hang is a distinct failure from a refusal.
        if [[ "${rc}" -eq 124 ]]; then
            echo "FAIL ${label}: timed out rather than failing"
            summary_record "${label}" 1
            return
        fi
        # 139/134: segfault or abort. Also not a refusal.
        if [[ "${rc}" -eq 139 || "${rc}" -eq 134 ]]; then
            echo "FAIL ${label}: crashed (exit ${rc}) rather than reporting an error"
            summary_record "${label}" 1
            return
        fi
    elif [[ "${rc}" -ne "${expect_exit}" ]]; then
        echo "FAIL ${label}: expected exit ${expect_exit}, got ${rc}"
        tail -5 "${log}" || true
        summary_record "${label}" 1
        return
    fi

    if [[ -n "${expect_msg}" ]] && ! grep -qi -- "${expect_msg}" "${log}"; then
        echo "FAIL ${label}: output does not mention '${expect_msg}'"
        tail -5 "${log}" || true
        summary_record "${label}" 1
        return
    fi

    echo "OK   ${label}"
    summary_record "${label}" 0
}

echo "MEDUSA CLI and diagnostics"
echo "  bin=${BIN}"

# ---------------------------------------------------------------------------
# Argument handling
# ---------------------------------------------------------------------------
check "cli-help"            0         "usage"   "${BIN}" --help
check "cli-bad-option"      nonzero   ""        "${BIN}" --no-such-option
check "cli-missing-file"    nonzero   "Invalid input file" \
      "${BIN}" --file "${WORKDIR}/does-not-exist.qasm"
check "cli-bad-nsamples"    nonzero   "number of samples" \
      "${BIN}" --file "${ROOT}/tests/qasm/metamorphic/identity_h2.qasm" --nsamples 12x
check "cli-file-needs-arg"  nonzero   ""        "${BIN}" --file

# fopen succeeds on a directory in read mode and the first read simply fails,
# so nothing is simulated. That used to exit 0 (issue #13); main now reports
# the failure.
check "cli-file-is-a-dir"   nonzero   ""        "${BIN}" --file "${ROOT}/tests"

# ---------------------------------------------------------------------------
# Parser diagnostics: malformed OpenQASM must be refused, not mis-simulated
# ---------------------------------------------------------------------------
mkdir -p "${WORKDIR}/bad"

cat >"${WORKDIR}/bad/unknown_gate.qasm" <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;
notagate q[0];
QASM

cat >"${WORKDIR}/bad/missing_qubits.qasm" <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
h q[0];
QASM

cat >"${WORKDIR}/bad/out_of_range.qasm" <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;
h q[9];
QASM

cat >"${WORKDIR}/bad/unterminated_loop.qasm" <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;
for int i in [1:4] {
h q[0];
QASM

cat >"${WORKDIR}/bad/stray_brace.qasm" <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;
h q[0];
}
QASM

cat >"${WORKDIR}/bad/empty.qasm" </dev/null

for bad in unknown_gate missing_qubits stray_brace; do
    check "parse-${bad}" nonzero "" "${BIN}" --file "${WORKDIR}/bad/${bad}.qasm"
done

# Both were accepted and exited 0 until issue #13 was fixed:
#
#   out_of_range       h q[9] on a 2-qubit register. The index was never
#                      checked against the declared width, so the gate was
#                      applied at a BDD variable level that was never
#                      allocated - two invalid reads under valgrind, in the
#                      probability walk and in the dot output, while res.dot
#                      still looked well-formed. get_q_idx now rejects it.
#   unterminated_loop  a `for` with no closing brace. The loop was silently
#                      dropped, so a truncated file simulated as a different
#                      circuit and said nothing.
#
# The message is asserted too, not just the exit status: for these two the
# whole point is that the user is told which qubit or which construct is
# wrong, rather than getting a bare failure.
check "parse-out_of_range"      nonzero "outside the declared register" \
      "${BIN}" --file "${WORKDIR}/bad/out_of_range.qasm"
check "parse-unterminated_loop" nonzero "inside a loop" \
      "${BIN}" --file "${WORKDIR}/bad/unterminated_loop.qasm"

# The highest valid index must still be accepted - the check is >=, and an
# off-by-one here would reject legitimate circuits.
cat >"${WORKDIR}/bad/edge_last_qubit.qasm" <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[3] q;
h q[2];
QASM
check "parse-highest-valid-index" 0 "" "${BIN}" --file "${WORKDIR}/bad/edge_last_qubit.qasm"

# ...and the first invalid one must be refused. q[9] above is far out of
# range, so it would still be caught by an off-by-one check; this is the case
# that pins >= rather than >.
cat >"${WORKDIR}/bad/edge_first_invalid.qasm" <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[3] q;
h q[3];
QASM
check "parse-first-invalid-index" nonzero "outside the declared register" \
      "${BIN}" --file "${WORKDIR}/bad/edge_first_invalid.qasm"

# An empty file declares no qubit register, so nothing is simulated and there
# is no result to report. That is a failure rather than a trivial success, and
# since #13 the exit status says so.
check "parse-empty" nonzero "" "${BIN}" --file "${WORKDIR}/bad/empty.qasm"

# ---------------------------------------------------------------------------
# Long-number output: algebraic coefficients past MAX_NUM_LEN (50 digits) are
# written to res-vars.txt as large-number[N] references instead of inline.
# Nothing exercised lnum_map_add / lnum_map_print before this.
# ---------------------------------------------------------------------------
LONG_CIRCUIT="${ROOT}/benchmarks/no-measure/LP-QuantumCounting/08_04_05_0.qasm"
if [[ -x "${GMP_BIN}" && -f "${LONG_CIRCUIT}" ]]; then
    label="gmp-long-numbers"
    log="${WORKDIR}/${label}.log"
    rc=0
    ( cd "${WORKDIR}" && timeout 300 "${GMP_BIN}" --file "${LONG_CIRCUIT}" >"${log}" 2>&1 ) || rc=$?
    if [[ "${rc}" -ne 0 ]]; then
        echo "FAIL ${label}: simulator exited ${rc}"
        tail -5 "${log}" || true
        summary_record "${label}" 1
    elif [[ ! -s "${WORKDIR}/res-vars.txt" ]]; then
        echo "FAIL ${label}: res-vars.txt not written - did MAX_NUM_LEN change?"
        summary_record "${label}" 1
    else
        # Every reference in the .dot must resolve to a definition in
        # res-vars.txt, and each definition must carry an actual long number.
        if python3 - "${WORKDIR}/res.dot" "${WORKDIR}/res-vars.txt" <<'PY'
import re, sys
dot  = open(sys.argv[1], encoding='utf-8', errors='replace').read()
vars_txt = open(sys.argv[2], encoding='utf-8', errors='replace').read()

refs = set(re.findall(r'large-number\[(\d+)\]', dot))
defs = dict(re.findall(r'large-number\[(\d+)\]\s*=\s*(-?\d+)', vars_txt))

if not refs:
    print("no large-number references in res.dot", file=sys.stderr); sys.exit(2)
missing = refs - set(defs)
if missing:
    print(f"{len(missing)} referenced ids have no definition: {sorted(missing)[:5]}", file=sys.stderr)
    sys.exit(3)
# The whole point is that these did not fit inline.
short = [k for k in refs if len(defs[k].lstrip('-')) <= 50]
if short:
    print(f"{len(short)} 'large' numbers are 50 digits or fewer", file=sys.stderr)
    sys.exit(4)
print(f"  {len(refs)} large numbers, all resolved, shortest {min(len(defs[k].lstrip('-')) for k in refs)} digits")
PY
        then
            echo "OK   ${label}"
            summary_record "${label}" 0
        else
            echo "FAIL ${label}: res.dot and res-vars.txt are inconsistent"
            summary_record "${label}" 1
        fi
    fi
else
    echo "SKIP gmp-long-numbers (build with: make buddy_gmp)"
fi

summary_print "test_cli"
exit $?
