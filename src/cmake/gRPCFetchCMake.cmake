#
# Copyright (c) 2022 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
#

include(FetchContent)

macro(apply_git_patch REPO_PATH PATCH_PATH)
  execute_process(COMMAND git apply --check ${PATCH_PATH}
    WORKING_DIRECTORY ${REPO_PATH}
    RESULT_VARIABLE SUCCESS
    ERROR_QUIET)

  if(${SUCCESS} EQUAL 0)
    message("Applying git patch ${PATCH_PATH} in ${REPO_PATH} repository")
    execute_process(COMMAND git am -3 ${PATCH_PATH}
      WORKING_DIRECTORY ${REPO_PATH}
      RESULT_VARIABLE SUCCESS)

    if(${SUCCESS} EQUAL 1)
      # We don't stop here because it can happen in case of parallel builds
      message(WARNING "\nError: failed to apply the patch patch: ${PATCH_PATH}\n")
    endif()
  else()
    message("Already applied: ${PATCH_PATH}")
  endif()
endmacro()

FetchContent_Declare(
  grpc
  GIT_REPOSITORY https://github.com/grpc/grpc
  GIT_TAG        v1.46.2
  GIT_SHALLOW 1
)
set(FETCHCONTENT_QUIET OFF)

FetchContent_GetProperties(grpc)
if (NOT grpc_POPULATED)
  FetchContent_Populate(grpc)

  #Apply VSOCK patches
  file(GLOB patches_files "${CMAKE_SOURCE_DIR}/src/external/patches/grpc/*.patch")
  foreach(file ${patches_files})
    apply_git_patch(${grpc_SOURCE_DIR} ${file})
  endforeach()

  add_subdirectory(${grpc_SOURCE_DIR} ${grpc_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

set(_PROTOBUF_LIBPROTOBUF libprotobuf)
set(_REFLECTION grpc++_reflection)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
set(_GRPC_GRPCPP grpc++)
set(_GRPC_CPP_PLUGIN_EXECUTABLE  $<TARGET_FILE:grpc_cpp_plugin>)
