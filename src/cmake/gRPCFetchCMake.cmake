#
# Copyright (c) 2022 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
#

include(FetchContent)

set(protobuf_INSTALL OFF)

FetchContent_Declare(
  grpc
  GIT_REPOSITORY https://github.com/grpc/grpc
  GIT_TAG        v1.65.1
  GIT_SHALLOW 1
)
set(FETCHCONTENT_QUIET OFF)

FetchContent_GetProperties(grpc)
if (NOT grpc_POPULATED)
  FetchContent_Populate(grpc)

  add_subdirectory(${grpc_SOURCE_DIR} ${grpc_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

set(_PROTOBUF_LIBPROTOBUF libprotobuf)
set(_REFLECTION grpc++_reflection)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
set(_GRPC_GRPCPP grpc++)
set(_GRPC_CPP_PLUGIN_EXECUTABLE  $<TARGET_FILE:grpc_cpp_plugin>)
