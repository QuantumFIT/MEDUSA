#!/usr/bin/env bash
# Integration smoke tests: run MEDUSA on small OpenQASM circuits.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=test_summary.sh
source "$(dirname "$0")/test_summary.sh"
summary_init

BIN="${ROOT}/MEDUSA_buddy_doubles_f128"

run_one() {
    local label="$1"
    local file="$2"
    local extra="${3:-}"
    if ! "${BIN}" --file "${file}" ${extra} >/tmp/medusa_test_out.txt 2>&1; then
        echo "FAIL ${label}: simulator exited non-zero"
        cat /tmp/medusa_test_out.txt
        summary_record "${label}" 1
        return
    fi
    if [[ ! -s "${ROOT}/res.dot" ]]; then
        echo "FAIL ${label}: missing/empty res.dot"
        summary_record "${label}" 1
        return
    fi
    echo "OK   ${label}"
    summary_record "${label}" 0
}

if [[ ! -x "${BIN}" ]]; then
    echo "Binary ${BIN} not found — build with: make buddy_doubles_f128"
    exit 1
fi

echo "MEDUSA circuit integration tests"

help_out="$(mktemp)"
if ! "${BIN}" --help >"${help_out}" 2>&1; then
    echo "FAIL help: --help exited non-zero"
    cat "${help_out}"
    summary_record "help" 1
elif ! grep -Eq -- '--norm-csv,[[:space:]]+-c' "${help_out}"; then
    echo "FAIL help: --norm-csv is not documented as -c"
    cat "${help_out}"
    summary_record "help" 1
elif grep -Eq -- '--norm-csv,[[:space:]]+-e' "${help_out}"; then
    echo "FAIL help: --norm-csv still listed as -e"
    cat "${help_out}"
    summary_record "help" 1
else
    echo "OK   help"
    summary_record "help" 0
fi
rm -f "${help_out}"

run_one "BV-01"          "${ROOT}/benchmarks/no-measure/BernsteinVazirani/01.qasm"
run_one "BV-02"          "${ROOT}/benchmarks/no-measure/BernsteinVazirani/02.qasm"
if [[ -f "${ROOT}/benchmarks/no-measure/LP-Grover/NL_06.qasm" ]]; then
    run_one "Grover-NL-small" "${ROOT}/benchmarks/no-measure/LP-Grover/NL_06.qasm"
else
    run_one "Grover-07"    "${ROOT}/benchmarks/no-measure/LP-Grover/07.qasm"
fi
run_one "MCToffoli-12"   "${ROOT}/benchmarks/no-measure/MCToffoli/12.qasm"

# Bell-like tiny circuit via stdin
cat > /tmp/medusa_bell.qasm <<'EOF'
OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
cx q[0],q[1];
EOF
if "${BIN}" --file /tmp/medusa_bell.qasm >/tmp/medusa_test_out.txt 2>&1 \
   && [[ -s "${ROOT}/res.dot" ]]; then
    echo "OK   bell-stdin"
    summary_record "bell-stdin" 0
else
    echo "FAIL bell-stdin"
    cat /tmp/medusa_test_out.txt || true
    summary_record "bell-stdin" 1
fi

if "${BIN}" --file /tmp/medusa_bell.qasm --probability >/tmp/medusa_test_out.txt 2>&1 \
   && [[ -s "${ROOT}/res.dot" ]] \
   && python3 - "${ROOT}/res.dot" <<'PY'
import re, sys
text = open(sys.argv[1], encoding="utf-8").read()
labs = re.findall(r'label="([^"]*)".*style=filled,shape=box', text)
if not labs:
    labs = re.findall(r'\[label="([^"]*)", style=filled,shape=box\]', text)
ok = False
for lab in labs:
    if lab == "0":
        continue
    if "i" in lab or "ω" in lab or "omega" in lab.lower():
        sys.exit(2)
    float(lab)
    ok = True
sys.exit(0 if ok else 1)
PY
then
    echo "OK   bell-probability"
    summary_record "bell-probability" 0
else
    echo "FAIL bell-probability: expected |amp|^2 floats on res.dot terminals"
    cat /tmp/medusa_test_out.txt || true
    summary_record "bell-probability" 1
fi

meas_out="$(mktemp)"
if "${BIN}" --file "${ROOT}/benchmarks/measure/BernsteinVazirani/01.qasm" \
        --measure "${meas_out}" --nsamples 8 >/tmp/medusa_test_out.txt 2>&1 \
        && [[ -s "${meas_out}" ]]; then
    echo "OK   BV-01-measure"
    summary_record "BV-01-measure" 0
else
    echo "FAIL BV-01-measure: expected non-empty measure file"
    cat /tmp/medusa_test_out.txt || true
    summary_record "BV-01-measure" 1
fi
rm -f "${meas_out}"

summary_print "test_circuits"
exit $?
