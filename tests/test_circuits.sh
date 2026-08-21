#!/usr/bin/env bash
# Integration smoke tests: run MEDUSA on small OpenQASM circuits.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=test_summary.sh
source "$(dirname "$0")/test_summary.sh"
summary_init

BIN="${ROOT}/MEDUSA_buddy_doubles_f64"

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
    echo "Binary ${BIN} not found — build with: make buddy_doubles_f64"
    exit 1
fi

echo "MEDUSA circuit integration tests"
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

summary_print "test_circuits"
exit $?
