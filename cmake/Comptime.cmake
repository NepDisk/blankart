cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

set(CMAKE_BINARY_DIR "${BINARY_DIR}")
set(CMAKE_CURRENT_BINARY_DIR "${BINARY_DIR}")

# Set up CMAKE path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

include(GitUtilities)

git_current_branch(SRB2_COMP_BRANCH)
git_working_tree_dirty(SRB2_COMP_UNCOMMITTED)

git_latest_commit(SRB2_COMP_REVISION)
git_subject(SRB2_COMP_LASTCOMMIT)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/src/config.h")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/build/src/config.h")
