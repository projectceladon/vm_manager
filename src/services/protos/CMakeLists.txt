#
# Copyright (c) 2022 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
#

cmake_minimum_required(VERSION 3.10)

file(GLOB CIV_PROTOS "*.proto")
list(APPEND PROTO_FLAGS -I${CMAKE_CURRENT_SOURCE_DIR})

file(REMOVE_RECURSE ${PROTOS_GEN_DIR})
file(MAKE_DIRECTORY ${PROTOS_GEN_DIR})

foreach(FIL ${CIV_PROTOS})
  get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
  get_filename_component(FIL_WE ${FIL} NAME_WE)

  list(APPEND CIV_PROTOS_SRCS "${PROTOS_GEN_DIR}/${FIL_WE}.pb.cc")
  list(APPEND CIV_PROTOS_HDRS "${PROTOS_GEN_DIR}/${FIL_WE}.pb.h")
  list(APPEND CIV_GRPC_PROTOS_SRCS "${PROTOS_GEN_DIR}/${FIL_WE}.grpc.pb.cc")
  list(APPEND CIV_GRPC_PROTOS_HDRS "${PROTOS_GEN_DIR}/${FIL_WE}.grpc.pb.h")

  add_custom_command(
      OUTPUT "${PROTOS_GEN_DIR}/${FIL_WE}.pb.cc"
             "${PROTOS_GEN_DIR}/${FIL_WE}.pb.h"
             "${PROTOS_GEN_DIR}/${FIL_WE}.grpc.pb.cc"
             "${PROTOS_GEN_DIR}/${FIL_WE}.grpc.pb.h"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${PROTOS_GEN_DIR}"
        --cpp_out "${PROTOS_GEN_DIR}"
        ${PROTO_FLAGS}
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        ${FIL})
endforeach()

include_directories("${PROTOS_GEN_DIR}")
add_library(vm_grpc_proto ${CIV_PROTOS_SRCS} ${CIV_PROTOS_HDRS} ${CIV_GRPC_PROTOS_SRCS} ${CIV_GRPC_PROTOS_HDRS})

target_link_libraries(vm_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF}
)
