# ==============================================================================
# MEDUSA tests under CTest.
#
# Labels map onto the CI jobs:
#   pr       tests.yml "MoToBuddy (default product)": make test,
#            test-unit-leaf-types, test-grover
#   cxx      tests.yml "C++ gate implementations": make test / test-grover
#            USE_CXX=1
#   memory   tests.yml "Stress and leak checks": test-stress, test-leaks
#            (valgrind, so `valgrind` must be installed)
#   sylvan   tests.yml "Sylvan": test-sylvan (needs -DMEDUSA_SYLVAN=ON)
#   nightly  nightly.yml: the slow suites
#
#   ctest --test-dir build/release -L pr -j8
#
# C harnesses run in their own directory under test-work/, with benchmarks/ and
# tests/ linked in (they open inputs relative to the repository root), so they
# can run in parallel. The shell suites cd into the source tree and write
# res.dot there, so they share a resource lock and run one at a time.
# ==============================================================================

set(TEST_SRC ${CMAKE_SOURCE_DIR}/tests)
set(WORK_ROOT ${CMAKE_BINARY_DIR}/test-work)

function(medusa_workdir out name)
  set(dir ${WORK_ROOT}/${name})
  file(MAKE_DIRECTORY ${dir})
  foreach(link benchmarks tests)
    if(NOT EXISTS ${dir}/${link})
      file(CREATE_LINK ${CMAKE_SOURCE_DIR}/${link} ${dir}/${link} SYMBOLIC)
    endif()
  endforeach()
  set(${out} ${dir} PARENT_SCOPE)
endfunction()

# medusa_harness(<target> <source> <variant> [defines...])
function(medusa_harness tgt src variant)
  if(NOT TARGET medusa_core_${variant})
    return()
  endif()
  add_executable(${tgt} ${TEST_SRC}/${src})
  target_link_libraries(${tgt} PRIVATE medusa_core_${variant})
  target_include_directories(${tgt} PRIVATE ${TEST_SRC})
  if(ARGN)
    target_compile_definitions(${tgt} PRIVATE ${ARGN})
  endif()
  set_target_properties(${tgt} PROPERTIES LINKER_LANGUAGE CXX)
  medusa_common_target_settings(${tgt})
endfunction()

# medusa_c_test(<name> <target> LABELS ... [TIMEOUT s] [WRAP cmd...])
function(medusa_c_test name tgt)
  cmake_parse_arguments(T "" "TIMEOUT" "LABELS;WRAP" ${ARGN})
  if(NOT TARGET ${tgt})
    return()
  endif()
  medusa_workdir(dir ${name})
  add_test(NAME ${name} COMMAND ${T_WRAP} $<TARGET_FILE:${tgt}> WORKING_DIRECTORY ${dir})
  if(NOT T_TIMEOUT)
    set(T_TIMEOUT 900)
  endif()
  set_tests_properties(${name} PROPERTIES LABELS "${T_LABELS}" TIMEOUT ${T_TIMEOUT})
endfunction()

# medusa_sh_test(<name> <script> LABELS ... [NEEDS targets...] [ENV k=v...] [TIMEOUT s])
function(medusa_sh_test name script)
  cmake_parse_arguments(T "" "TIMEOUT" "LABELS;NEEDS;ENV" ${ARGN})
  foreach(t IN LISTS T_NEEDS)
    if(NOT TARGET ${t})
      return()
    endif()
  endforeach()
  add_test(NAME ${name} COMMAND bash ${TEST_SRC}/${script}
           WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  if(NOT T_TIMEOUT)
    set(T_TIMEOUT 900)
  endif()
  set_tests_properties(${name} PROPERTIES LABELS "${T_LABELS}" TIMEOUT ${T_TIMEOUT}
                       RESOURCE_LOCK medusa_source_tree ENVIRONMENT "${T_ENV}")
endfunction()

set(BIN ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# ------------------------------------------------------------------------------
# MoToBuddy C harnesses, C and C++ gate paths
# ------------------------------------------------------------------------------
set(_gate_paths "")
list(APPEND _gate_paths "|pr")
if(MEDUSA_CXX_GATES)
  list(APPEND _gate_paths "_cxx|cxx")
endif()

foreach(_gp IN LISTS _gate_paths)
  string(REPLACE "|" ";" _gp "${_gp}")
  list(GET _gp 0 sfx)
  list(GET _gp 1 lbl)

  # test_unit_api for every float leaf type (make test-unit-leaf-types); the f128
  # binary keeps the Makefile's name. The C++ job only ever ran it on f128.
  foreach(ft IN LISTS MEDUSA_FLOAT_TYPES)
    if(sfx STREQUAL "_cxx" AND NOT ft STREQUAL "f128")
      continue()
    endif()
    if(ft STREQUAL "f128")
      set(t test_unit_api${sfx})
    else()
      set(t test_unit_api_${ft}${sfx})
    endif()
    medusa_harness(${t} test_unit_api.c buddy_doubles_${ft}${sfx})
    medusa_c_test(unit_api_${ft}${sfx} ${t} LABELS ${lbl})
  endforeach()

  medusa_harness(test_benchmark_semantics${sfx} test_benchmark_semantics.c buddy_doubles_f128${sfx})
  medusa_c_test(benchmark_semantics${sfx} test_benchmark_semantics${sfx} LABELS ${lbl})

  medusa_harness(test_metamorphic${sfx} test_metamorphic.c buddy_doubles_f128${sfx})
  medusa_c_test(metamorphic${sfx} test_metamorphic${sfx} LABELS ${lbl})

  medusa_harness(test_unit_leaf_gmp${sfx} test_unit_leaf_gmp.c buddy_gmp${sfx})
  medusa_c_test(unit_leaf_gmp${sfx} test_unit_leaf_gmp${sfx} LABELS ${lbl})

  # Grover matrix: every float type and GMP (make test-grover).
  foreach(l IN LISTS MEDUSA_FLOAT_TYPES ITEMS gmp)
    if(l STREQUAL "gmp")
      set(v buddy_gmp${sfx})
    else()
      set(v buddy_doubles_${l}${sfx})
    endif()
    medusa_harness(test_grover_${l}${sfx} test_grover_matrix.c ${v})
    medusa_c_test(grover_${l}${sfx} test_grover_${l}${sfx} LABELS ${lbl})
  endforeach()
endforeach()

medusa_harness(test_unit_htab test_unit_htab.c buddy_doubles_f128)
medusa_c_test(unit_htab test_unit_htab LABELS pr)

# ------------------------------------------------------------------------------
# Shell suites (make test-cli / test-mosf / test-circuits / test-benchmarks)
# ------------------------------------------------------------------------------
foreach(_gp IN LISTS _gate_paths)
  string(REPLACE "|" ";" _gp "${_gp}")
  list(GET _gp 0 sfx)
  list(GET _gp 1 lbl)
  set(f128 ${BIN}/MEDUSA_buddy_doubles_f128${sfx})
  set(gmp  ${BIN}/MEDUSA_buddy_gmp${sfx})
  medusa_sh_test(cli${sfx} test_cli.sh LABELS ${lbl}
    NEEDS medusa_buddy_doubles_f128${sfx} medusa_buddy_gmp${sfx}
    ENV MEDUSA_BIN=${f128} MEDUSA_GMP_BIN=${gmp})
  medusa_sh_test(circuits${sfx} test_circuits.sh LABELS ${lbl}
    NEEDS medusa_buddy_doubles_f128${sfx} ENV MEDUSA_BIN=${f128})
  medusa_sh_test(rotation_cache${sfx} test_rotation_cache.sh LABELS ${lbl}
    NEEDS medusa_buddy_doubles_f128${sfx} ENV MEDUSA_BIN=${f128})
  medusa_sh_test(benchmarks${sfx} test_benchmarks.sh LABELS ${lbl}
    NEEDS medusa_buddy_doubles_f128${sfx} ENV MEDUSA_BIN=${f128})
  # test_mosf.sh skips itself on a binary without MOSF, as under plain `make test`.
  medusa_sh_test(mosf${sfx} test_mosf.sh LABELS ${lbl}
    NEEDS medusa_buddy_doubles_f128${sfx} ENV MEDUSA_BIN=${f128})
endforeach()

# ------------------------------------------------------------------------------
# Stress and leaks (make test-stress / test-leaks)
# ------------------------------------------------------------------------------
medusa_harness(test_stress_f128 test_stress.c buddy_doubles_f128 STRESS_LEVEL=${MEDUSA_STRESS_LEVEL})
medusa_c_test(stress_f128 test_stress_f128 LABELS memory TIMEOUT 1800)
medusa_harness(test_stress_gmp test_stress.c buddy_gmp STRESS_LEVEL=${MEDUSA_STRESS_LEVEL})
medusa_c_test(stress_gmp test_stress_gmp LABELS memory TIMEOUT 1800)

# test_leaks.sh's checks, one CTest test each: same valgrind flags, and the
# stress binary at STRESS_LEVEL=1 as that script builds it.
find_program(VALGRIND valgrind)
if(VALGRIND)
  set(VG ${VALGRIND} --leak-check=full --show-leak-kinds=all
      --errors-for-leak-kinds=definite,indirect,possible,reachable
      --error-exitcode=42 --quiet)
  medusa_harness(test_stress_f128_l1 test_stress.c buddy_doubles_f128 STRESS_LEVEL=1)
  foreach(t test_unit_api test_stress_f128_l1 test_benchmark_semantics test_metamorphic)
    medusa_c_test(leaks_${t} ${t} LABELS memory TIMEOUT 3600 WRAP ${VG})
  endforeach()
  if(TARGET medusa_buddy_doubles_f128)
    medusa_workdir(dir leaks_symbolic_grover05)
    add_test(NAME leaks_symbolic_grover05
             COMMAND ${VG} $<TARGET_FILE:medusa_buddy_doubles_f128> --symbolic
                     --file ${CMAKE_SOURCE_DIR}/benchmarks/no-measure/LP-Grover/05.qasm
             WORKING_DIRECTORY ${dir})
    set_tests_properties(leaks_symbolic_grover05 PROPERTIES LABELS memory TIMEOUT 1800)
  endif()
endif()

# ------------------------------------------------------------------------------
# Nightly
# ------------------------------------------------------------------------------
medusa_sh_test(lp_symbolic test_lp_symbolic.sh LABELS nightly TIMEOUT 3600
  NEEDS medusa_buddy_doubles_f128 ENV MEDUSA_BIN=${BIN}/MEDUSA_buddy_doubles_f128)

# ------------------------------------------------------------------------------
# Sylvan (make test-sylvan, test-sylvan-leaf-types, test-grover-sylvan;
# test-sylvan-metamorphic is nightly)
# ------------------------------------------------------------------------------
if(MEDUSA_SYLVAN)
  set(syl ${BIN}/MEDUSA_sylvan_doubles_f128)
  medusa_sh_test(sylvan_circuits test_circuits.sh LABELS sylvan
    NEEDS medusa_sylvan_doubles_f128 ENV MEDUSA_BIN=${syl})
  medusa_sh_test(sylvan_rotation_cache test_rotation_cache.sh LABELS sylvan
    NEEDS medusa_sylvan_doubles_f128 ENV MEDUSA_BIN=${syl})
  medusa_sh_test(sylvan_benchmarks test_benchmarks.sh LABELS sylvan
    NEEDS medusa_sylvan_doubles_f128 ENV MEDUSA_BIN=${syl} MEDUSA_TEST_TIMEOUT=90)
  medusa_sh_test(sylvan_compare test_sylvan.sh LABELS sylvan
    NEEDS medusa_sylvan_doubles_f128 medusa_sylvan_gmp medusa_buddy_doubles_f128 medusa_buddy_gmp
    ENV MEDUSA_SYLVAN_F128=${syl} MEDUSA_SYLVAN_GMP=${BIN}/MEDUSA_sylvan_gmp
        MEDUSA_BUDDY_F128=${BIN}/MEDUSA_buddy_doubles_f128 MEDUSA_BUDDY_GMP=${BIN}/MEDUSA_buddy_gmp)
  foreach(l IN LISTS MEDUSA_FLOAT_TYPES ITEMS gmp)
    if(l STREQUAL "gmp")
      set(v sylvan_gmp)
    else()
      set(v sylvan_doubles_${l})
      medusa_harness(test_benchmark_semantics_sylvan_${l} test_benchmark_semantics.c ${v})
      medusa_c_test(sylvan_benchmark_semantics_${l} test_benchmark_semantics_sylvan_${l} LABELS sylvan)
      if(l STREQUAL "f80" OR l STREQUAL "f128")
        medusa_harness(test_metamorphic_sylvan_${l} test_metamorphic.c ${v})
        medusa_c_test(sylvan_metamorphic_${l} test_metamorphic_sylvan_${l}
                      LABELS "nightly;sylvan" TIMEOUT 1800)
      endif()
    endif()
    medusa_harness(test_grover_sylvan_${l} test_grover_matrix.c ${v})
    medusa_c_test(sylvan_grover_${l} test_grover_sylvan_${l} LABELS sylvan)
  endforeach()
endif()
