#!/usr/bin/env bash
# The SoftFloat f128 build (SOFTFLOAT=1) against the native binary128 one, on
# the same circuits: res.dot (amplitudes and, with --probability, |amp|^2) and
# the --norm-csv columns must be identical to the last printed digit.
#
# Only where both builds exist, i.e. x86_64 Linux: the CI's SoftFloat job runs
# it from the platform probe. Not under --symbolic, where (1/sqrt 2)^k differs
# by design (exact powers of two vs powf128).
#
# Usage: tests/test_softfloat_vs_native.sh SOFT_BINARY NATIVE_BINARY
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOFT="$(realpath "$1")"
NATIVE="$(realpath "$2")"
# shellcheck source=test_summary.sh
source "${ROOT}/tests/test_summary.sh"
summary_init

WORKDIR="${ROOT}/.test-work/softfloat_vs_native_$$"
mkdir -p "${WORKDIR}"
trap 'rm -rf "${WORKDIR}"' EXIT

# run BIN DIR CIRCUIT ARGS... - simulate in DIR (res.dot lands there)
run() {
    local bin="$1" dir="$2" circ="$3"; shift 3
    mkdir -p "${dir}"
    (cd "${dir}" && "${bin}" --file "${circ}" --norm-error --norm-csv norm.csv "$@" > stdout.txt 2>&1)
}

compare() {  # name circuit args...
    local name="$1" circ="${ROOT}/$2"; shift 2
    local d="${WORKDIR}/${name}"
    if ! run "${SOFT}" "${d}/soft" "${circ}" "$@" || ! run "${NATIVE}" "${d}/native" "${circ}" "$@"; then
        echo "FAIL ${name}: a simulation failed"; summary_record "${name}" 1; return
    fi
    local bad=""
    for f in res.dot norm.csv; do
        if [[ ! -s "${d}/soft/${f}" || ! -s "${d}/native/${f}" ]]; then
            bad+=" ${f}(missing)"
        elif ! cmp -s "${d}/soft/${f}" "${d}/native/${f}"; then
            bad+=" ${f}"
        fi
    done
    if [[ -z "${bad}" ]]; then
        echo "OK   ${name}  ($(grep -c 'label' "${d}/soft/res.dot") labels, $(wc -l < "${d}/soft/norm.csv") norm rows)"
        summary_record "${name}" 0
    else
        echo "FAIL ${name}: differs in${bad}"
        for f in res.dot norm.csv; do
            diff "${d}/soft/${f}" "${d}/native/${f}" 2>&1 | head -4 || true
        done
        summary_record "${name}" 1
    fi
}

compare BV-01          benchmarks/no-measure/BernsteinVazirani/01.qasm
compare BV-05          benchmarks/no-measure/BernsteinVazirani/05.qasm
compare MCToffoli-05   benchmarks/no-measure/MCToffoli/05.qasm
compare Feynman-tof3   benchmarks/no-measure/Feynman/tof_3.qasm
compare Feynman-mux3   benchmarks/no-measure/Feynman/csla_mux_3.qasm
compare LP-Grover-05   benchmarks/no-measure/LP-Grover/05.qasm
compare Random-03      benchmarks/no-measure/Random/03.qasm
compare Random-06      benchmarks/no-measure/Random/06.qasm
compare RevLib-xor5    benchmarks/no-measure/RevLib/xor5_254.qasm
compare Random-06-prob benchmarks/no-measure/Random/06.qasm --probability

summary_print "test_softfloat_vs_native"
exit $?
