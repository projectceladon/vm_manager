#
# Copyright (c) 2022 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
#

set(build_version_ "unknown")

find_package(Git)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --long
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE build_version_
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if("${build_version_}" STREQUAL "")
    execute_process(
      COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      OUTPUT_VARIABLE build_version_
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  endif()
else()
  message(STATUS "Git not found!")
endif()
message(STATUS "Build version: ${build_version_}")

set(build_type_ ${CMAKE_BUILD_TYPE})
message(STATUS "BUILD Type: ${build_type_}")

string(TIMESTAMP time_stamp_ UTC)
message(STATUS "BUILD TimeStamp: ${time_stamp_}")

cmake_host_system_information(RESULT build_fqdn_ QUERY FQDN)
message(STATUS "BUILD FQDN: ${build_fqdn_}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/revision.h.in ${CMAKE_CURRENT_BINARY_DIR}/revision.h @ONLY)
