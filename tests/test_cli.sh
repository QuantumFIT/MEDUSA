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

# Measurement targets are bounded too, and the classical index gets its own
# diagnostic (get_c_idx): c[5] on a 2-bit register must be reported as a *bit*
# index, not a qubit one. Both statement forms reach the check - the
# `measure q -> c` form reads the qubit first, the OpenQASM 3 `c = measure q`
# form reads the bit first - so each gets a rejected and an accepted case.
for form in arrow assign; do
    if [[ "${form}" == arrow ]]; then
        bad_stmt='measure q[0] -> c[5];'; good_stmt='measure q[1] -> c[1];'
    else
        bad_stmt='c[5] = measure q[0];';  good_stmt='c[1] = measure q[1];'
    fi
    printf 'OPENQASM 3.0;\ninclude "stdgates.inc";\nqubit[2] q;\nbit[2] c;\nh q[0];\n%s\n' \
        "${bad_stmt}" >"${WORKDIR}/bad/measure_bit_${form}.qasm"
    printf 'OPENQASM 3.0;\ninclude "stdgates.inc";\nqubit[2] q;\nbit[2] c;\nh q[0];\n%s\n' \
        "${good_stmt}" >"${WORKDIR}/bad/measure_ok_${form}.qasm"
    check "parse-measure-bit-out-of-range-${form}" nonzero "bit index c\[5\] is outside" \
          "${BIN}" --file "${WORKDIR}/bad/measure_bit_${form}.qasm"
    check "parse-measure-last-bit-${form}" 0 "" \
          "${BIN}" --file "${WORKDIR}/bad/measure_ok_${form}.qasm"
done

# An empty file declares no qubit register, so nothing is simulated and there
# is no result to report. That is a failure rather than a trivial success, and
# since #13 the exit status says so.
check "parse-empty" nonzero "" "${BIN}" --file "${WORKDIR}/bad/empty.qasm"

# The bit register may come first: both size checks then run with the other
# register already declared.
printf 'OPENQASM 3.0;\ninclude "stdgates.inc";\nbit[2] c;\nqubit[2] q;\nx q[1];\nc[1] = measure q[1];\n' \
    >"${WORKDIR}/bit_register_first.qasm"
check "bit-register-first" 0 "" "${BIN}" --file "${WORKDIR}/bit_register_first.qasm"

# A comment holding a 0xFF byte ("caf\xff", Latin-1 "café"). The comment
# skipper kept fgetc's result in a char, so with a signed char that byte read
# as EOF: parsing stopped there and the rest of the circuit was silently
# dropped.
printf 'OPENQASM 2.0;\n// caf\xff\nqreg q[1];\nx q[0];\n' >"${WORKDIR}/byte_ff_comment.qasm"
check "comment-with-0xff-byte" 0 "" "${BIN}" --file "${WORKDIR}/byte_ff_comment.qasm"

# ---------------------------------------------------------------------------
# Parser error paths.
#
# sim.c carries ~38 distinct error_exit sites and almost none were reached:
# 81.1% line, 67.2% branch, and 65 partials in the Codecov report, because
# every other suite feeds it a well-formed circuit. Each case below asserts
# the *specific* diagnostic, not merely a non-zero exit - a parser that
# refuses everything with one generic message is not much better than one
# that accepts everything.
# ---------------------------------------------------------------------------
write_bad() {  # name, content on stdin
    cat >"${WORKDIR}/bad/$1.qasm"
}

write_bad num_nondigit <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[x] q;
QASM

write_bad num_toolong <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[123456789012345678901234567890] q;
QASM

write_bad two_qregs <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;
qubit[3] r;
QASM

write_bad two_bregs <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;
bit[2] c;
bit[2] d;
QASM

write_bad reg_mismatch_bit_first <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
bit[2] c;
qubit[3] q;
QASM

write_bad reg_mismatch <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;
bit[3] c;
QASM

write_bad nested_loop <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;
for int i in [1:2] {
for int j in [1:2] {
h q[0];
}
}
QASM

write_bad loop_step_zero <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;
for int i in [1:0:4] {
h q[0];
}
QASM

write_bad loop_not_integer <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;
for int i in [1:2:5] {
h q[0];
}
QASM

write_bad loop_backwards <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;
for int i in [4:1] {
h q[0];
}
QASM

write_bad measure_no_breg <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;
h q[0];
measure q[0] -> c[0];
QASM

write_bad mcx_syntax <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[4] q;
mcx q[0] q[1], q[2];
QASM

write_bad rx_angle <<'QASM'
OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;
rx(notanangle) q[0];
QASM

# name:expected-diagnostic
while IFS='|' read -r fixture want; do
    [ -z "${fixture}" ] && continue
    check "parse-${fixture}" nonzero "${want}" "${BIN}" --file "${WORKDIR}/bad/${fixture}.qasm"
done <<'CASES'
num_nondigit|non-digit character
num_toolong|too many digits
two_qregs|Multiple qubit register
two_bregs|Multiple bit register
reg_mismatch|different than the size
reg_mismatch_bit_first|different than the size
nested_loop|Nested loops
loop_step_zero|step must be non-zero
loop_not_integer|not an integer
loop_backwards|Invalid number of loop iterations
measure_no_breg|uninitialized bit register
mcx_syntax|mcx
rx_angle|Invalid rx angle
CASES

# A gate before any register declaration is caught earlier, by the `if (init)`
# guard around gate parsing - which is why get_q_idx carries no such check.
check "parse-gate-before-register" nonzero "Circuit not initialized" \
      "${BIN}" --file "${WORKDIR}/bad/missing_qubits.qasm"

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

# A write error on res-vars.txt is reported, not ignored: the file is a link
# to /dev/full, so writing the large numbers fails partway (Linux only). This
# output is ~30 KB, so the error shows while writing; the short-output cases
# below fail only when the file is closed.
if [[ -x "${GMP_BIN}" && -f "${LONG_CIRCUIT}" && -c /dev/full ]]; then
    label="gmp-long-numbers-write-error"
    full_dir="${WORKDIR}/full"
    mkdir -p "${full_dir}"
    ln -sf /dev/full "${full_dir}/res-vars.txt"
    log="${WORKDIR}/${label}.log"
    rc=0
    ( cd "${full_dir}" && timeout 300 "${GMP_BIN}" --file "${LONG_CIRCUIT}" >/dev/null 2>"${log}" ) || rc=$?
    if [[ "${rc}" -eq 1 ]] && grep -q "error occurred when writing the value of large-number" "${log}"; then
        echo "OK   ${label}"
        summary_record "${label}" 0
    else
        echo "FAIL ${label}: exit ${rc}, expected 1 and a write error"
        tail -5 "${log}" || true
        summary_record "${label}" 1
    fi
fi

# write_error_case LABEL BIN FILE EXPECTED_MESSAGE QASM: FILE is a link to
# /dev/full and the output is short enough to sit in the stdio buffer until
# fclose; MEDUSA must still exit 1 with the message.
write_error_case() {
    local label="$1" bin="$2" file="$3" expect="$4" qasm="$5"
    local dir="${WORKDIR}/${label}" log="${WORKDIR}/${label}.log" rc=0
    mkdir -p "${dir}"
    printf '%s' "${qasm}" > "${dir}/c.qasm"
    ln -sf /dev/full "${dir}/${file}"
    ( cd "${dir}" && timeout 60 "${bin}" --file c.qasm >/dev/null 2>"${log}" ) || rc=$?
    if [[ "${rc}" -eq 1 ]] && grep -q "${expect}" "${log}"; then
        echo "OK   ${label}"
        summary_record "${label}" 0
    else
        echo "FAIL ${label}: exit ${rc}, expected 1 and \"${expect}\""
        tail -5 "${log}" || true
        summary_record "${label}" 1
    fi
}
if [[ -c /dev/full ]]; then
    # ~800 bytes of large numbers: (1/sqrt 2)^1000-ish coefficients
    if [[ -x "${GMP_BIN}" ]]; then
        write_error_case gmp-short-res-vars-write-error "${GMP_BIN}" res-vars.txt \
            "error occurred when writing res-vars.txt" \
            $'OPENQASM 3.0;\ninclude "stdgates.inc";\nqubit[1] q;\nfor int i in [1:1000] {\nh q[0];\nt q[0];\n}\n'
    fi
    write_error_case short-res-dot-write-error "${BIN}" res.dot \
        "error occurred when writing res.dot" \
        $'OPENQASM 3.0;\ninclude "stdgates.inc";\nqubit[1] q;\nx q[0];\n'
fi

summary_print "test_cli"
exit $?
