# FetchContent PATCH_COMMAND for Sylvan v1.8.1 (run in its source directory).
#
# src/CMakeLists.txt configures its package-config template from
# ${CMAKE_SOURCE_DIR}/cmake/, which only exists when Sylvan is the top-level
# project. As a subproject that is MEDUSA's source dir and configure fails.
# PROJECT_SOURCE_DIR is Sylvan's own root either way. Idempotent.
set(f src/CMakeLists.txt)
file(READ ${f} content)
string(REPLACE "\${CMAKE_SOURCE_DIR}/cmake/sylvan-config.cmake.in"
               "\${PROJECT_SOURCE_DIR}/cmake/sylvan-config.cmake.in" patched "${content}")
if(NOT patched STREQUAL content)
  file(WRITE ${f} "${patched}")
elseif(NOT content MATCHES "PROJECT_SOURCE_DIR}/cmake/sylvan-config.cmake.in")
  # Neither the original line nor the patched one: Sylvan changed (e.g. a new
  # MEDUSA_SYLVAN_TAG). Fail here, at the commit that caused it, instead of
  # with Sylvan's own CMAKE_SOURCE_DIR error later.
  message(FATAL_ERROR "patch_sylvan.cmake: ${f} has no sylvan-config.cmake.in line to patch; "
                      "update the script for this Sylvan version")
endif()
