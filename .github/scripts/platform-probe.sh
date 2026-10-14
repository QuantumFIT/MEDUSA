#!/usr/bin/env bash
# Probe which configurations build and pass on the current platform.
#
# For every leaf type (f32, f64, f80, f128, gmp) this builds MEDUSA, then runs
# the unit suite, the Grover matrix (amplitude-level checks, classic and
# --symbolic) and the circuit smoke test against that binary. Each step is
# recorded as pass / FAIL / skip (a step whose build failed), a Markdown table
# goes to $GITHUB_STEP_SUMMARY (or stdout), and failing logs are tailed.
#
# Exit status: 0 when every failure is expected, 1 otherwise. EXPECT_FAIL holds
# the step names (as printed, e.g. "grover-f32") expected to fail on this
# platform; an expected failure that passes is reported, so the list does not
# go stale silently. PLATFORM labels the summary.
#
# Run from the repository root, after `make init`. Honours CC, CXX and JOBS.
set -u

JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)}"
EXPECT_FAIL=" ${EXPECT_FAIL:-} "
MAKE_ARGS=(--no-print-directory -j"${JOBS}")
[ -n "${CC:-}" ] && MAKE_ARGS+=("CC=${CC}")
[ -n "${CXX:-}" ] && MAKE_ARGS+=("CXX=${CXX}")
LOGS="probe-logs"; mkdir -p "${LOGS}"

names=(); results=(); unexpected=0

# step NAME CMD... - run CMD, log it, record the outcome.
step() {
    local name="$1"; shift
    local t0=$SECONDS r
    if "$@" > "${LOGS}/${name}.log" 2>&1; then r=pass; else r=FAIL; fi
    local dt=$((SECONDS - t0))
    if [ "${r}" = FAIL ]; then
        if [[ "${EXPECT_FAIL}" == *" ${name} "* ]]; then
            r="FAIL (expected)"
        else
            unexpected=1
            echo "::group::${name} failed - last 40 lines"
            tail -40 "${LOGS}/${name}.log"
            echo "::endgroup::"
        fi
    elif [[ "${EXPECT_FAIL}" == *" ${name} "* ]]; then
        r="pass (expected to fail)"
        echo "::warning::${name} passes here but is listed in EXPECT_FAIL"
    fi
    echo "${name}: ${r} (${dt}s)"
    names+=("${name}"); results+=("${r}")
    [[ "${r}" == pass* ]]
}

skip() { names+=("$1"); results+=("skip"); echo "$1: skip"; }

for spec in f32:0 f64:1 f80:2 f128:3; do
    s="${spec%%:*}"; n="${spec##*:}"
    if step "build-${s}" make "${MAKE_ARGS[@]}" buddy_doubles LEAF_FLOAT_TYPE="${n}"; then
        # test_unit_api has one name for every leaf type; never reuse another's.
        rm -f test_unit_api
        step "unit-${s}" bash -c 'make "$@" test_unit_api LEAF_FLOAT_TYPE='"${n}"' && ./test_unit_api' _ "${MAKE_ARGS[@]}"
        step "grover-${s}" make "${MAKE_ARGS[@]}" "test-grover-${s}"
        step "circuits-${s}" env MEDUSA_BIN="./MEDUSA_buddy_doubles_${s}" bash tests/test_circuits.sh
    else
        skip "unit-${s}"; skip "grover-${s}"; skip "circuits-${s}"
    fi
done

if step build-gmp make "${MAKE_ARGS[@]}" buddy_gmp; then
    step unit-gmp make "${MAKE_ARGS[@]}" test-unit-gmp
    step grover-gmp make "${MAKE_ARGS[@]}" test-grover-gmp
    step circuits-gmp env MEDUSA_BIN=./MEDUSA_buddy_gmp bash tests/test_circuits.sh
else
    skip unit-gmp; skip grover-gmp; skip circuits-gmp
fi

{
    cc="${CC:-gcc}"
    gmp=$(echo '#include <gmp.h>' | "${cc}" -dM -E -x c - 2>/dev/null | awk '
          $2 == "__GNU_MP_VERSION" { a = $3 } $2 == "__GNU_MP_VERSION_MINOR" { b = $3 }
          $2 == "__GNU_MP_VERSION_PATCHLEVEL" { c = $3 } END { if (a != "") print a "." b "." c }')
    echo "### ${PLATFORM:-$(uname -sm)}"
    echo
    echo "$("${cc}" --version 2>/dev/null | head -1); $(cmake --version 2>/dev/null | head -1); GMP ${gmp:-?}; $(uname -m)"
    echo
    echo "| leaf | build | unit | grover | circuits |"
    echo "|---|---|---|---|---|"
    for s in f32 f64 f80 f128 gmp; do
        row="| ${s} "
        for k in build unit grover circuits; do
            for i in "${!names[@]}"; do
                [ "${names[$i]}" = "${k}-${s}" ] && row+="| ${results[$i]} "
            done
        done
        echo "${row}|"
    done
    echo
} | tee -a "${GITHUB_STEP_SUMMARY:-/dev/null}"

exit "${unexpected}"
