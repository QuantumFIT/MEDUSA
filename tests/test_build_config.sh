#!/usr/bin/env bash
# Build-configuration checks: what the Makefile does on compilers and C
# libraries other than the one CI runs. Each is checked here on Linux/GCC by
# simulating the other platform, so that a regression shows up in every run,
# not only in the platform probe:
#
#   fat-lto-gcc      a GCC release build compiles with -ffat-lto-objects
#                    (fat objects let a later `make LTO=0` relink without a
#                    rebuild), and says nothing about it
#   fat-lto-apple    a compiler that rejects -ffat-lto-objects next to -flto,
#                    as Apple clang does, still builds - without the flag and
#                    without the warning meant for GCC
#   fat-lto-warning  GCC whose test compile fails gets the warning, once
#   f128-no-quadmath an f128 build where <quadmath.h> is missing (clang on
#                    Linux, macOS) stops with the #error that names another
#                    leaf type, not with "file not found"
#   gmp-no-quadmath  the GMP links do not pass -lquadmath, which macOS does
#                    not have (checked on the commands: a Linux linker with
#                    --as-needed drops it from the binary either way), and the
#                    GMP binaries do not depend on it
#
# Run from the repository root (make test-build-config). The last check needs
# the GMP binaries, which make test-unit-gmp builds. The simulations of GCC need
# `gcc` to be GNU GCC; where it is not (macOS: Apple clang), they are skipped.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${ROOT}"
# shellcheck source=test_summary.sh
source "${ROOT}/tests/test_summary.sh"
summary_init

WORKDIR="${ROOT}/.test-work/build_config_$$"
mkdir -p "${WORKDIR}"
trap 'rm -rf "${WORKDIR}"' EXIT

pass() { echo "OK   $1"; summary_record "$1" 0; }
fail() { echo "FAIL $1: $2"; summary_record "$1" 1; }
skip() { echo "SKIP $1 ($2)"; }

# GNU GCC, as the Makefile tells it from clang: __GNUC__ without __clang__.
GNU_GCC=0
if gcc -dM -E -x c /dev/null 2> /dev/null \
   | awk '$2 == "__clang__" { c = 1 } $2 == "__GNUC__" { g = 1 } END { exit !(g && !c) }'; then
    GNU_GCC=1
fi

# The makes below must run as top-level makes even under `make
# test-build-config`: the Makefile's warning is printed by the top level only.
make() { env -u MAKELEVEL -u MAKEFLAGS -u MFLAGS make "$@"; }

# The compile commands make would run for TARGET, one per line (make -n prints
# them with backslash continuations), and its own messages (warnings) on stderr.
dry_run() {
    local out="$1"; shift
    make --no-print-directory -n -B "$@" > "${out}.cmds" 2> "${out}.err" || true
    sed -e ':a' -e '/\\$/N; s/\\\n//; ta' "${out}.cmds" | grep -- ' -c src/' > "${out}.cc" || true
}

# ---------------------------------------------------------------------------
# Fat LTO objects.
# ---------------------------------------------------------------------------
if [[ "${GNU_GCC}" -eq 1 ]]; then
    dry_run "${WORKDIR}/gcc" buddy_doubles_f64 BUILD=release LTO=1 CC=gcc
    n=$(wc -l < "${WORKDIR}/gcc.cc")
    fat=$(grep -c -- '-ffat-lto-objects' "${WORKDIR}/gcc.cc" || true)
    if [[ "${n}" -gt 0 && "${fat}" -eq "${n}" ]] && ! grep -q 'failed a test compile' "${WORKDIR}/gcc.err"; then
        pass "fat-lto-gcc"
    else
        fail "fat-lto-gcc" "${fat} of ${n} compile commands have -ffat-lto-objects"
    fi
else
    skip "fat-lto-gcc" "gcc is not GNU GCC"
fi

# A stand-in for Apple clang: GCC underneath, but it rejects -ffat-lto-objects
# together with -flto (as Apple clang does) and identifies as clang.
APPLE_CC="${WORKDIR}/apple-cc"
cat > "${APPLE_CC}" <<'SH'
#!/bin/sh
lto=0; fat=0; dm=0
for a in "$@"; do
    case "$a" in -flto*) lto=1 ;; -ffat-lto-objects) fat=1 ;; -dM) dm=1 ;; esac
done
if [ "$lto" = 1 ] && [ "$fat" = 1 ]; then
    echo "clang: error: unsupported option '-ffat-lto-objects'" >&2
    exit 1
fi
if [ "$dm" = 1 ]; then exec gcc -D__clang__=1 "$@"; fi
exec gcc "$@"
SH
chmod +x "${APPLE_CC}"
dry_run "${WORKDIR}/apple" buddy_doubles_f64 BUILD=release LTO=1 CC="${APPLE_CC}"
n=$(wc -l < "${WORKDIR}/apple.cc")
fat=$(grep -c -- '-ffat-lto-objects' "${WORKDIR}/apple.cc" || true)
first=$(head -1 "${WORKDIR}/apple.cc")
# And one of those commands really compiles with it, into a scratch object.
if [[ "${n}" -gt 0 && "${fat}" -eq 0 ]] && ! grep -q 'failed a test compile' "${WORKDIR}/apple.err" \
   && eval "${first% -o *} -o ${WORKDIR}/probe.o" 2> "${WORKDIR}/apple.cc.err"; then
    pass "fat-lto-apple"
else
    fail "fat-lto-apple" "${fat} of ${n} commands have -ffat-lto-objects; warning: $(grep -c 'failed a test compile' "${WORKDIR}/apple.err" || true)"
    tail -3 "${WORKDIR}/apple.cc.err" 2>/dev/null || true
fi

# GCC whose test compile fails for some other reason (no LTO plugin, ...).
BROKEN_CC="${WORKDIR}/broken-gcc"
cat > "${BROKEN_CC}" <<'SH'
#!/bin/sh
for a in "$@"; do [ "$a" = -ffat-lto-objects ] && exit 1; done
exec gcc "$@"
SH
chmod +x "${BROKEN_CC}"
if [[ "${GNU_GCC}" -eq 1 ]]; then
    dry_run "${WORKDIR}/broken" buddy_doubles_f64 BUILD=release LTO=1 CC="${BROKEN_CC}"
    warnings=$(grep -c 'failed a test compile' "${WORKDIR}/broken.err" || true)
    if [[ "${warnings}" -eq 1 ]]; then
        pass "fat-lto-warning"
    else
        fail "fat-lto-warning" "expected one warning, got ${warnings}"
    fi
else
    skip "fat-lto-warning" "gcc is not GNU GCC"
fi

# ---------------------------------------------------------------------------
# f128 without <quadmath.h>: GCC's own include directory, minus quadmath*.h,
# in place of the real one - what clang on Linux sees.
# ---------------------------------------------------------------------------
if [[ "${GNU_GCC}" -eq 1 ]]; then
    GCC_INC="$(gcc -print-file-name=include)"
    NOQM="${WORKDIR}/gcc-include-no-quadmath"
    mkdir -p "${NOQM}"
    for f in "${GCC_INC}"/*; do
        case "$(basename "$f")" in quadmath*) ;; *) ln -s "$f" "${NOQM}/" ;; esac
    done
    SYS_INC=()
    while read -r d; do
        [ "$d" = "${GCC_INC}" ] || SYS_INC+=(-isystem "$d")
    done < <(echo | gcc -E -Wp,-v -x c - 2>&1 | sed -n '/<...> search starts here/,/End of search/p' | grep '^ /' | sed 's/^ //')
    if [ -f "${GCC_INC}/quadmath.h" ]; then
        printf '#include "leaf_primitive_double.h"\n' > "${WORKDIR}/f128.c"
        gcc -nostdinc -isystem "${NOQM}" "${SYS_INC[@]}" \
            -I src/interface_implementations/leaf_primitives -DLEAF_FLOAT_TYPE=3 \
            -fsyntax-only "${WORKDIR}/f128.c" > "${WORKDIR}/f128.log" 2>&1 || true
        if grep -q 'needs GCC.s libquadmath' "${WORKDIR}/f128.log" \
           && ! grep -q 'quadmath.h.*No such file' "${WORKDIR}/f128.log"; then
            pass "f128-no-quadmath"
        else
            fail "f128-no-quadmath" "expected the libquadmath #error"
            head -5 "${WORKDIR}/f128.log"
        fi
    else
        skip "f128-no-quadmath" "this GCC has no quadmath.h to take away"
    fi
else
    skip "f128-no-quadmath" "gcc is not GNU GCC"
fi

# ---------------------------------------------------------------------------
# GMP builds must not depend on libquadmath.
# ---------------------------------------------------------------------------
deps() {
    if command -v readelf > /dev/null; then readelf -d "$1"
    elif command -v otool > /dev/null; then otool -L "$1"
    else return 1
    fi
}
# The link commands first: the default leaf type is f128, so CLIBS carries
# -lquadmath, and a GMP link that used it would fail on macOS.
make --no-print-directory -n -B buddy_gmp ./test_unit_leaf_gmp ./test_stress_gmp \
    2> /dev/null | sed -e ':a' -e '/\\$/N; s/\\\n//; ta' \
    | grep -E -- '-o [^ ]*(MEDUSA_buddy_gmp|test_unit_leaf_gmp|test_stress_gmp)( |$)' \
    | grep -v -- ' -c ' > "${WORKDIR}/gmp-links" || true
links=$(wc -l < "${WORKDIR}/gmp-links")
withqm=$(grep -c -- '-lquadmath' "${WORKDIR}/gmp-links" || true)
bad=""
checked=0
[[ "${links}" -ge 3 ]] || bad+=" (found ${links} of 3 GMP link commands)"
[[ "${withqm}" -eq 0 ]] || bad+=" (${withqm} GMP link commands pass -lquadmath)"
for b in MEDUSA_buddy_gmp test_unit_leaf_gmp; do
    [ -x "$b" ] || continue
    checked=$((checked + 1))
    out=$(deps "$b") || { bad+=" $b(no readelf/otool)"; continue; }
    grep -q quadmath <<< "${out}" && bad+=" $b"
done
if [[ "${checked}" -eq 0 ]]; then
    fail "gmp-no-quadmath" "no GMP binaries found (run make buddy_gmp test-unit-gmp first)"
elif [[ -n "${bad}" ]]; then
    fail "gmp-no-quadmath" "linked against libquadmath:${bad}"
else
    pass "gmp-no-quadmath"
fi

summary_print "test_build_config"
exit $?
