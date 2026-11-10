# Configures MEDUSA in a scratch build tree and checks the outcome.
#
#   cmake -DSRC=<source dir> -DBUILD=<scratch build dir>
#         -DGENERATOR=<generator> -DMAKE_PROGRAM=<path>
#         -DC_COMPILER=<path> -DCXX_COMPILER=<path>
#         [-DARGS=<-Dx=y|-Dz=w...>]           extra cache settings, '|'-separated
#         [-DEXPECT_FAIL=ON]                   configuring must fail
#         [-DEXPECT=<regex|regex...>]          each must match the configure output
#         [-DEXPECT_DISABLED=<test|test...>]   each must be a disabled CTest test
#         -P check_configure.cmake
#
# The compilers and the make program are passed explicitly: the scratch tree
# must not depend on what happens to be on PATH.

foreach(v SRC BUILD GENERATOR MAKE_PROGRAM C_COMPILER CXX_COMPILER)
  if(NOT ${v})
    message(FATAL_ERROR "check_configure.cmake: ${v} is not set")
  endif()
endforeach()
foreach(v ARGS EXPECT EXPECT_DISABLED)
  string(REPLACE "|" ";" ${v} "${${v}}")
endforeach()

file(REMOVE_RECURSE "${BUILD}")
execute_process(
  COMMAND ${CMAKE_COMMAND} -S ${SRC} -B ${BUILD} -G ${GENERATOR}
          -DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}
          -DCMAKE_C_COMPILER=${C_COMPILER} -DCMAKE_CXX_COMPILER=${CXX_COMPILER}
          ${ARGS}
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
message("${out}")

if(EXPECT_FAIL AND rc EQUAL 0)
  message(FATAL_ERROR "configuring succeeded, but it should have failed")
elseif(NOT EXPECT_FAIL AND NOT rc EQUAL 0)
  message(FATAL_ERROR "configuring failed: ${rc}")
endif()

foreach(re IN LISTS EXPECT)
  if(NOT out MATCHES "${re}")
    message(FATAL_ERROR "the configure output does not match: ${re}")
  endif()
endforeach()

if(EXPECT_DISABLED)
  execute_process(COMMAND ${CMAKE_CTEST_COMMAND} --show-only=json-v1
                  WORKING_DIRECTORY ${BUILD}
                  RESULT_VARIABLE rc OUTPUT_VARIABLE json ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "ctest --show-only failed: ${err}")
  endif()
  string(JSON n LENGTH "${json}" tests)
  math(EXPR last "${n} - 1")
  foreach(want IN LISTS EXPECT_DISABLED)
    set(state "not registered")
    foreach(i RANGE ${last})
      string(JSON name GET "${json}" tests ${i} name)
      if(NOT name STREQUAL want)
        continue()
      endif()
      set(state "enabled")
      string(JSON np ERROR_VARIABLE noprops LENGTH "${json}" tests ${i} properties)
      if(noprops)
        break()
      endif()
      math(EXPR plast "${np} - 1")
      foreach(j RANGE ${plast})
        string(JSON pname GET "${json}" tests ${i} properties ${j} name)
        string(JSON pval GET "${json}" tests ${i} properties ${j} value)
        if(pname STREQUAL "DISABLED" AND pval)
          set(state "disabled")
        endif()
      endforeach()
      break()
    endforeach()
    if(NOT state STREQUAL "disabled")
      message(FATAL_ERROR "test ${want} should be registered and disabled, but is ${state}")
    endif()
  endforeach()
endif()
