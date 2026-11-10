# Runs a command while holding an exclusive lock on a file.
#
#   cmake -DLOCK=<file> [-DLOCK_TIMEOUT=<seconds>] -P run_locked.cmake <command> [args...]
#
# The shell test suites run in the source tree and write res.dot there.
# CTest's RESOURCE_LOCK keeps them apart within one ctest run, but not across
# build trees: `ctest --preset pr & ctest --preset sylvan &` on one checkout
# would let one suite read the other's res.dot. A lock on a file in the source
# tree holds across build trees and ctest invocations. file(LOCK) works the
# same on Linux and macOS, which flock(1) does not.
#
# Arguments must not contain ';', which CMake reads as a list separator (the
# test suites pass a script path and nothing else).

if(NOT LOCK)
  message(FATAL_ERROR "run_locked.cmake: LOCK is not set")
endif()
if(NOT LOCK_TIMEOUT)
  set(LOCK_TIMEOUT 3600)
endif()

# The command is everything after the script name.
set(_cmd "")
set(_script_seen FALSE)
math(EXPR _last "${CMAKE_ARGC} - 1")
foreach(_i RANGE 1 ${_last})
  if(_script_seen)
    list(APPEND _cmd "${CMAKE_ARGV${_i}}")
  elseif(CMAKE_ARGV${_i} STREQUAL "-P")
    math(EXPR _next "${_i} + 1")
    set(_skip ${_next})
  elseif(DEFINED _skip AND _i EQUAL _skip)
    set(_script_seen TRUE)
  endif()
endforeach()
if(NOT _cmd)
  message(FATAL_ERROR "run_locked.cmake: no command given")
endif()

file(LOCK "${LOCK}" GUARD PROCESS TIMEOUT ${LOCK_TIMEOUT} RESULT_VARIABLE _lock)
if(NOT _lock EQUAL 0)
  message(FATAL_ERROR "run_locked.cmake: could not lock ${LOCK} within ${LOCK_TIMEOUT} s: ${_lock}")
endif()

# Output is not captured, so it reaches ctest as the command's own.
execute_process(COMMAND ${_cmd} RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
  list(JOIN _cmd " " _shown)
  message(FATAL_ERROR "${_shown} failed: ${_rc}")
endif()
