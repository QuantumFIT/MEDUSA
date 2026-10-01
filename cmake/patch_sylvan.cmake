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
endif()
