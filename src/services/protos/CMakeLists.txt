#
# Copyright (c) 2022 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
#

cmake_minimum_required(VERSION 3.10)

get_filename_component(vm_host_proto "vm_host.proto" ABSOLUTE)
get_filename_component(vm_host_proto_path "${vm_host_proto}" PATH)

set(PROTOS_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/gens)

set(vm_host_proto_srcs "${PROTOS_GEN_DIR}/vm_host.pb.cc")
set(vm_host_proto_hdrs "${PROTOS_GEN_DIR}/vm_host.pb.h")
set(vm_host_grpc_srcs "${PROTOS_GEN_DIR}/vm_host.grpc.pb.cc")
set(vm_host_grpc_hdrs "${PROTOS_GEN_DIR}/vm_host.grpc.pb.h")

add_custom_command(
      OUTPUT "${vm_host_proto_srcs}" "${vm_host_proto_hdrs}" "${vm_host_grpc_srcs}" "${vm_host_grpc_hdrs}"
      COMMAND ${CMAKE_COMMAND} -E remove_directory ${PROTOS_GEN_DIR}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTOS_GEN_DIR}
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${PROTOS_GEN_DIR}"
        --cpp_out "${PROTOS_GEN_DIR}"
        -I "${vm_host_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${vm_host_proto}"
      DEPENDS "${vm_host_proto}")

# Include generated *.pb.h files
include_directories("${PROTOS_GEN_DIR}")

# vm_host_grpc_proto
add_library(vm_host_grpc_proto
  ${vm_host_grpc_srcs}
  ${vm_host_grpc_hdrs}
  ${vm_host_proto_srcs}
  ${vm_host_proto_hdrs})

target_link_libraries(vm_host_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})
