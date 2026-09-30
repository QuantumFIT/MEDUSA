# ==============================================================================
# MEDUSA - CMake build
#
# Every variant of the simulator is its own target, and all of them can be built
# and tested from one build tree:
#
#   cmake -B build/release                     # Release: -O2 -g + LTO
#   cmake --build build/release -j
#   ctest --test-dir build/release -L pr -j    # what CI runs on every PR
#
#   cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug                 # -O0 -g
#   cmake -B build/coverage -DCMAKE_BUILD_TYPE=Debug -DMEDUSA_COVERAGE=ON
#
# Variants (backend x leaf type x gate implementation):
#   medusa_buddy_doubles_{f32,f64,f80,f128}   MEDUSA_buddy_doubles_<t>
#   medusa_buddy_gmp                          MEDUSA_buddy_gmp
#   ..._cxx                                   the same with the C++ gate path
#                                             (and MOSF for the float builds)
#   medusa_sylvan_doubles_<t>, medusa_sylvan_gmp   with -DMEDUSA_SYLVAN=ON
#
# The Makefile remains the reference build until CI has moved over; flags here
# match it: the MEDUSA sources get -O2 -g (Release) or -O0 -g
# -fno-omit-frame-pointer (Debug), no -DNDEBUG, and LTO in Release; MoToBuddy
# and Sylvan keep CMake's Release flags (-O3 -DNDEBUG), or RelWithDebInfo's in
# Debug, exactly as `make init` builds them.
# ==============================================================================

cmake_minimum_required(VERSION 3.21)
project(MEDUSA LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Release or Debug" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Release Debug)
if(NOT CMAKE_BUILD_TYPE MATCHES "^(Release|Debug)$")
  message(FATAL_ERROR "CMAKE_BUILD_TYPE must be Release or Debug, not '${CMAKE_BUILD_TYPE}'")
endif()

# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------
set(MEDUSA_FLOAT_TYPES "f32;f64;f80;f128" CACHE STRING
    "Float leaf types to build (subset of f32;f64;f80;f128)")
option(MEDUSA_GMP        "Build the algebraic GMP leaf variants"               ON)
option(MEDUSA_CXX_GATES  "Also build every variant with the C++ gate path"     ON)
option(MEDUSA_SYLVAN     "Build the optional Sylvan backend variants"          OFF)
option(MEDUSA_LTO        "Link-time optimisation in Release"                   ON)
option(MEDUSA_COVERAGE   "gcov instrumentation (requires CMAKE_BUILD_TYPE=Debug)" OFF)
option(MEDUSA_DEBUG_LOG  "Compile in the MEDUSA_DEBUG runtime event log"       OFF)
option(MEDUSA_TESTS      "Build the test harnesses and register CTest tests"   ON)
set(MEDUSA_LEAF_ABS_EPS "" CACHE STRING "Override LEAF_ABS_EPS (empty = per-type default)")
set(MEDUSA_LEAF_REL_EPS "" CACHE STRING "Override LEAF_REL_EPS (empty = per-type default)")
set(MEDUSA_STRESS_LEVEL 2 CACHE STRING "STRESS_LEVEL for the stress-test harnesses")

set(MEDUSA_MOTOBUDDY_REPO   "https://github.com/VeriFIT/MoToBuddy.git" CACHE STRING "")
# Keep in sync with MOTOBUDDY_COMMIT in the Makefile while both builds exist.
set(MEDUSA_MOTOBUDDY_COMMIT "32ec8c6b9966fadd3cc3ed249ce478ea7639b46b" CACHE STRING "")
set(MEDUSA_SYLVAN_TAG       "v1.8.1" CACHE STRING "")

if(MEDUSA_COVERAGE AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(FATAL_ERROR "MEDUSA_COVERAGE=ON needs CMAKE_BUILD_TYPE=Debug (line attribution needs -O0)")
endif()

# ------------------------------------------------------------------------------
# Dependencies. Their flags are fixed here, before they are added: CMake's
# Release defaults (-O3 -DNDEBUG), and RelWithDebInfo's (-O2 -g -DNDEBUG) in a
# Debug build - what `make init` / `make init BUILD=debug` produce. The
# MEDUSA flags are set further down; per-config flag variables are directory
# scoped, so the dependencies keep these.
# ------------------------------------------------------------------------------
set(CMAKE_C_FLAGS_DEBUG   "-O2 -g -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O2 -g -DNDEBUG")

include(FetchContent)
set(BUDDY_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(motobuddy
  GIT_REPOSITORY ${MEDUSA_MOTOBUDDY_REPO}
  GIT_TAG        ${MEDUSA_MOTOBUDDY_COMMIT})
FetchContent_MakeAvailable(motobuddy)

if(MEDUSA_SYLVAN)
  set(SYLVAN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SYLVAN_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
  # Sylvan's package-config step assumes it is the top-level project; the
  # patch makes it subproject-safe (see cmake/patch_sylvan.cmake). A local
  # FETCHCONTENT_SOURCE_DIR_SYLVAN skips PATCH_COMMAND, so point that at a
  # tree the script has been run on. Lace (v1.4.1) is fetched by Sylvan itself.
  FetchContent_Declare(sylvan
    GIT_REPOSITORY https://github.com/trolando/sylvan.git
    GIT_TAG        ${MEDUSA_SYLVAN_TAG}
    PATCH_COMMAND  ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/patch_sylvan.cmake
    UPDATE_DISCONNECTED ON)
  FetchContent_MakeAvailable(sylvan)
endif()

find_library(GMP_LIBRARY gmp REQUIRED)
find_path(GMP_INCLUDE_DIR gmp.h REQUIRED)
find_library(QUADMATH_LIBRARY quadmath
             HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
if("f128" IN_LIST MEDUSA_FLOAT_TYPES AND NOT QUADMATH_LIBRARY)
  message(FATAL_ERROR "f128 needs libquadmath")
endif()
find_package(Threads REQUIRED)

# ------------------------------------------------------------------------------
# MEDUSA flags (this directory and below)
# ------------------------------------------------------------------------------
set(CMAKE_C_FLAGS_RELEASE   "-O2 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")
set(CMAKE_C_FLAGS_DEBUG     "-O0 -g -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g -fno-omit-frame-pointer")

set(MEDUSA_IPO OFF)
if(MEDUSA_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
  include(CheckIPOSupported)
  check_ipo_supported(RESULT MEDUSA_IPO OUTPUT _ipo_msg LANGUAGES C CXX)
  if(NOT MEDUSA_IPO)
    message(WARNING "LTO requested but not supported: ${_ipo_msg}")
  endif()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(SRC        ${CMAKE_SOURCE_DIR}/src)
set(IFACE      ${SRC}/interface_implementations)
set(BACKENDS   ${IFACE}/backends)
set(LEAF_PRIM  ${IFACE}/leaf_primitives)
set(LEAVES     ${IFACE}/leaves)

file(GLOB MEDUSA_ENGINE_SOURCES CONFIGURE_DEPENDS ${SRC}/*.c)
list(REMOVE_ITEM MEDUSA_ENGINE_SOURCES ${SRC}/main.c)
if(NOT MEDUSA_DEBUG_LOG)
  # medusa_debug.h provides inline no-ops when MEDUSA_DEBUG is off.
  list(REMOVE_ITEM MEDUSA_ENGINE_SOURCES ${SRC}/medusa_debug.c)
endif()

# The C++ gate path compiles gates.c / gates_symb.c as C++. A source file has
# one language per directory, and the C variants need them as C, so the C++
# variants compile one-line wrappers that include the originals. Includes inside
# gates.c still resolve relative to src/, as with the Makefile's `g++ -x c++`.
set(_cxx_wrap_dir ${CMAKE_BINARY_DIR}/cxx_gates)
foreach(_g gates gates_symb)
  file(CONFIGURE OUTPUT ${_cxx_wrap_dir}/${_g}.cpp
       CONTENT "/* generated: ${_g}.c compiled as C++ (MEDUSA C++ gate path) */\n#include \"${SRC}/${_g}.c\"\n")
endforeach()

# Applied to every MEDUSA target (simulators and test harnesses).
function(medusa_common_target_settings tgt)
  if(MEDUSA_IPO)
    set_property(TARGET ${tgt} PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)
  endif()
  if(MEDUSA_COVERAGE)
    target_compile_options(${tgt} PRIVATE --coverage)
    target_link_options(${tgt} PRIVATE --coverage)
  endif()
endfunction()

# ------------------------------------------------------------------------------
# medusa_variant(<name> BACKEND buddy|sylvan LEAF f32|f64|f80|f128|gmp [CXX])
#
# Defines
#   medusa_cfg_<name>   INTERFACE: include order, -D / -include, link libraries
#   medusa_core_<name>  STATIC: everything except main.c (what tests link)
#   medusa_<name>       the simulator, OUTPUT_NAME MEDUSA_<name>
# ------------------------------------------------------------------------------
function(medusa_variant name)
  cmake_parse_arguments(V "CXX" "BACKEND;LEAF" "" ${ARGN})

  if(V_LEAF STREQUAL "gmp")
    set(_leaf_hdr ${LEAF_PRIM}/leaf_primitive_mpz.h)
    set(_leaf_src ${LEAF_PRIM}/leaf_primitive_mpz.c ${LEAVES}/leaf_algebraic_mpz.c)
    set(_defs LEAF_BACKEND_GMP)
  else()
    string(REPLACE "f32" "0" _ft "${V_LEAF}")
    string(REPLACE "f64" "1" _ft "${_ft}")
    string(REPLACE "f80" "2" _ft "${_ft}")
    string(REPLACE "f128" "3" _ft "${_ft}")
    set(_leaf_hdr ${LEAF_PRIM}/leaf_primitive_double.h)
    set(_leaf_src ${LEAF_PRIM}/leaf_primitive_double.c ${LEAVES}/leaf_reim_double.c)
    set(_defs LEAF_BACKEND_DOUBLES LEAF_FLOAT_TYPE=${_ft})
    if(MEDUSA_LEAF_ABS_EPS)
      list(APPEND _defs LEAF_ABS_EPS=${MEDUSA_LEAF_ABS_EPS})
    endif()
    if(MEDUSA_LEAF_REL_EPS)
      list(APPEND _defs LEAF_REL_EPS=${MEDUSA_LEAF_REL_EPS})
    endif()
  endif()

  set(cfg medusa_cfg_${name})
  add_library(${cfg} INTERFACE)
  if(V_BACKEND STREQUAL "sylvan")
    set(_iface_hdr ${BACKENDS}/interface_sylvan.h)
    set(_iface_src ${BACKENDS}/interface_sylvan.c)
    list(APPEND _defs SYLVAN_BACKEND)
    target_include_directories(${cfg} INTERFACE ${BACKENDS}/sylvan_compat)
    target_link_libraries(${cfg} INTERFACE sylvan::sylvan)
  else()
    set(_iface_hdr ${BACKENDS}/interface_motobuddy.h)
    set(_iface_src ${BACKENDS}/interface_motobuddy.c)
    # MoToBuddy's headers (bdd.h, mtbdd.h) must come before src/: src/mtbdd.h
    # would otherwise shadow lib's mtbdd.h for files outside src/.
    target_include_directories(${cfg} INTERFACE
      ${motobuddy_SOURCE_DIR}/src ${motobuddy_BINARY_DIR}/src ${CMAKE_BINARY_DIR}/include)
    target_link_libraries(${cfg} INTERFACE buddy)
  endif()
  target_include_directories(${cfg} INTERFACE
    ${SRC} ${IFACE}/inc ${BACKENDS} ${LEAF_PRIM} ${LEAVES} ${GMP_INCLUDE_DIR})
  if(MEDUSA_DEBUG_LOG)
    list(APPEND _defs MEDUSA_DEBUG)
  endif()
  target_compile_definitions(${cfg} INTERFACE ${_defs})
  target_compile_options(${cfg} INTERFACE
    "SHELL:-include ${_leaf_hdr}" "SHELL:-include ${_iface_hdr}"
    $<$<COMPILE_LANGUAGE:CXX>:-fext-numeric-literals>)
  target_link_libraries(${cfg} INTERFACE ${GMP_LIBRARY} Threads::Threads m)
  if(V_LEAF STREQUAL "f128" OR V_LEAF STREQUAL "gmp")
    # GMP builds also link it: the Makefile always does for LEAF_FLOAT_TYPE=3.
    if(QUADMATH_LIBRARY)
      target_link_libraries(${cfg} INTERFACE ${QUADMATH_LIBRARY})
    endif()
  endif()

  set(_engine ${MEDUSA_ENGINE_SOURCES})
  if(V_CXX)
    list(REMOVE_ITEM _engine ${SRC}/gates.c ${SRC}/gates_symb.c)
    list(APPEND _engine ${_cxx_wrap_dir}/gates.cpp ${_cxx_wrap_dir}/gates_symb.cpp)
  endif()
  set(core medusa_core_${name})
  add_library(${core} STATIC ${_engine} ${_iface_src} ${_leaf_src})
  target_link_libraries(${core} PUBLIC ${cfg})
  set_target_properties(${core} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
                                           CXX_EXTENSIONS OFF)
  medusa_common_target_settings(${core})

  set(exe medusa_${name})
  set(_main ${SRC}/main.c)
  if(V_CXX AND NOT V_LEAF STREQUAL "gmp" AND V_BACKEND STREQUAL "buddy")
    # MOSF (JSON input) exists only on the MoToBuddy float builds.
    list(APPEND _main ${SRC}/sim_mosf.cpp)
  endif()
  add_executable(${exe} ${_main})
  target_link_libraries(${exe} PRIVATE ${core})
  if(V_CXX AND NOT V_LEAF STREQUAL "gmp" AND V_BACKEND STREQUAL "buddy")
    target_compile_definitions(${exe} PRIVATE USE_MOSF)
  endif()
  set_target_properties(${exe} PROPERTIES OUTPUT_NAME MEDUSA_${name}
                                          CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
                                          CXX_EXTENSIONS OFF LINKER_LANGUAGE CXX)
  medusa_common_target_settings(${exe})
  set_property(GLOBAL APPEND PROPERTY MEDUSA_VARIANTS ${name})
endfunction()

set(_leaves ${MEDUSA_FLOAT_TYPES})
if(MEDUSA_GMP)
  list(APPEND _leaves gmp)
endif()
foreach(_l IN LISTS _leaves)
  if(_l STREQUAL "gmp")
    set(_n gmp)
  else()
    set(_n doubles_${_l})
  endif()
  medusa_variant(buddy_${_n} BACKEND buddy LEAF ${_l})
  if(MEDUSA_CXX_GATES)
    medusa_variant(buddy_${_n}_cxx BACKEND buddy LEAF ${_l} CXX)
  endif()
  if(MEDUSA_SYLVAN)
    medusa_variant(sylvan_${_n} BACKEND sylvan LEAF ${_l})
  endif()
endforeach()

# ./MEDUSA, as `make` creates it.
if(TARGET medusa_buddy_doubles_f128)
  add_custom_command(TARGET medusa_buddy_doubles_f128 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink MEDUSA_buddy_doubles_f128
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/MEDUSA
    VERBATIM)
endif()

if(MEDUSA_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

get_property(_variants GLOBAL PROPERTY MEDUSA_VARIANTS)
message(STATUS "MEDUSA ${CMAKE_BUILD_TYPE}: LTO=${MEDUSA_IPO} coverage=${MEDUSA_COVERAGE} "
               "debug-log=${MEDUSA_DEBUG_LOG}")
message(STATUS "MEDUSA variants: ${_variants}")
