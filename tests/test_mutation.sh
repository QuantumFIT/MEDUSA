#!/usr/bin/env bash
# Targeted mutation testing: reintroduce known bugs and expect the suite to
# kill them (tests must FAIL). Restores sources after each mutant.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${ROOT}"

# shellcheck source=test_summary.sh
source "${ROOT}/tests/test_summary.sh"
summary_init

NPROC="$(nproc 2>/dev/null || echo 4)"
WORKDIR="${TMPDIR:-/tmp}/medusa_mut_$$"
mkdir -p "${WORKDIR}/bak"
trap 'restore_all; rm -rf "${WORKDIR}"' EXIT

MUT_FILES=(
    src/gates.c
    src/interface_implementations/leaves/leaf_reim_double.c
    src/interface_implementations/backends/interface_motobuddy.c
)

restore_all() {
    local f
    for f in "${MUT_FILES[@]}"; do
        local base
        base="$(basename "$f")"
        if [[ -f "${WORKDIR}/bak/${base}" ]]; then
            cp -f "${WORKDIR}/bak/${base}" "${ROOT}/${f}"
        fi
    done
}

backup_files() {
    local f
    for f in "${MUT_FILES[@]}"; do
        cp -f "${ROOT}/${f}" "${WORKDIR}/bak/$(basename "$f")"
    done
}

apply_mut() {
    local file="$1" old="$2" new="$3"
    python3 - "$ROOT/$file" "$old" "$new" <<'PY'
import sys
path, old, new = sys.argv[1], sys.argv[2], sys.argv[3]
text = open(path, encoding="utf-8").read()
n = text.count(old)
if n != 1:
    sys.stderr.write(f"mutant apply failed in {path}: expected 1 occurrence, found {n}\n")
    sys.exit(2)
open(path, "w", encoding="utf-8").write(text.replace(old, new, 1))
PY
}

rebuild_unit() {
    rm -f obj/buddy_doubles_f128/gates.o \
          obj/buddy_doubles_f128/mtbdd.o \
          obj/buddy_doubles_f128/interface_motobuddy.o \
          obj/leaf_reim_double_f128.o \
          test_unit_api \
          test_grover_f128
    make buddy_doubles_f128 test_unit_api LEAF_FLOAT_TYPE=3 -j"${NPROC}" >/dev/null 2>&1
}

run_unit() { ./test_unit_api >/dev/null 2>&1; }

run_grover_f128() {
    # Rebuild grover binary against mutated objects already built by rebuild_unit
    rm -f test_grover_f128
    make ./test_grover_f128 LEAF_FLOAT_TYPE=3 -j"${NPROC}" >/dev/null 2>&1
    ./test_grover_f128 >/dev/null 2>&1
}

record_mutant() {
    local id="$1" killed="$2"
    if [[ "${killed}" -eq 1 ]]; then
        echo "  KILLED   ${id}"
        summary_record "${id}" 0
    else
        echo "  SURVIVED ${id}"
        summary_record "${id}" 1
    fi
}

run_mutant() {
    local id="$1" file="$2" old="$3" new="$4" check="$5"
    echo
    echo ">>> mutant: ${id}"
    restore_all
    if ! apply_mut "${file}" "${old}" "${new}"; then
        echo "  APPLY-FAIL ${id}"
        summary_record "${id} (apply failed)" 1
        restore_all
        return
    fi
    if ! rebuild_unit; then
        # Build failure counts as killed (mutant is invalid / caught by compile)
        echo "  KILLED   ${id} (build failed)"
        summary_record "${id}" 0
        restore_all
        return
    fi
    local killed=0
    case "${check}" in
        unit)
            if ! run_unit; then killed=1; fi
            ;;
        unit_or_grover)
            if ! run_unit; then
                killed=1
            elif ! run_grover_f128; then
                killed=1
            fi
            ;;
        *)
            echo "unknown check: ${check}"; exit 2
            ;;
    esac
    restore_all
    record_mutant "${id}" "${killed}"
}

backup_files

echo "MEDUSA targeted mutation testing"
echo "(KILLED = suite detected the bug; SURVIVED = gap in tests)"

run_mutant "gate_x_inverted_skip" \
    "src/gates.c" \
    'if (!qBDD_isTerminal(*p_t) && xt < qBDD_level(*p_t)) {
        return;
    }' \
    'if (xt >= qBDD_level(*p_t)) {
        return;
    }' \
    "unit_or_grover"

run_mutant "addLeaf_alias_null_operand" \
    "src/interface_implementations/leaves/leaf_reim_double.c" \
    'if (a.pImpl == NULL) return clonePimpl(b);
    if (b.pImpl == NULL) return clonePimpl(a);' \
    'if (a.pImpl == NULL) return (b);
    if (b.pImpl == NULL) return (a);' \
    "unit"

run_mutant "classic_freefun_unregistered" \
    "src/interface_implementations/backends/interface_motobuddy.c" \
    'mtbdd_register_free_function(lt_classic, freePimpl);' \
    '/* mutant: mtbdd_register_free_function(lt_classic, freePimpl); */' \
    "unit"

run_mutant "toffoli_endif_epilogue" \
    "src/gates.c" \
    '    qBDD_unprotect(*p_t);
    qBDD_protect(res);
    *p_t = res;
#endif

    if (g_norm_track_enabled) {
        char label[32];
        snprintf(label, sizeof(label), "Toffoli(%u,%u,%u)", xt, xc1, xc2);' \
    '#endif

    qBDD_unprotect(*p_t);
    qBDD_protect(res);
    *p_t = res;

    if (g_norm_track_enabled) {
        char label[32];
        snprintf(label, sizeof(label), "Toffoli(%u,%u,%u)", xt, xc1, xc2);' \
    "unit"

run_mutant "toffoli_drop_result_assign" \
    "src/gates.c" \
    '    qBDD_unprotect(res);
    qBDD_unprotect(inter_res3);
    qBDD_unprotect(*p_t);
    *p_t = res2;
    
#else' \
    '    qBDD_unprotect(res);
    qBDD_unprotect(inter_res3);
    qBDD_unprotect(*p_t);
    /* mutant: *p_t = res2; */
    
#else' \
    "unit"

restore_all
# Leave tree clean and binaries consistent with clean sources
rebuild_unit >/dev/null 2>&1 || true

echo
# summary_print: PASS = mutant killed; FAIL = survived
# Remap labels for clarity in the table
summary_print "test_mutation"
exit $?
