#
# Copyright (c) 2022 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
#
cmake_minimum_required(VERSION 3.10)

set(APPLAUNCHER_PROJ "app-launcher-client")
project(${APPLAUNCHER_PROJ})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unused-result -Wno-unused-variable -Wno-narrowing -O2")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g1 -O3 -fstack-protector-strong -Wdate-time -D_FORTIFY_SOURCE=2" CACHE STRING "CXX Release Flags" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O2 -g3")

include_directories(${PROTOS_GEN_DIR})
include_directories(${DEPS_INCS_INCLUDE_DIRS})
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${EP_BOOST_INC_DIR})

file(GLOB applauncher_src app_launcher_client.cc)

add_executable(${APPLAUNCHER_PROJ} ${applauncher_src})
add_dependencies(${APPLAUNCHER_PROJ} vm_grpc_proto)

target_link_libraries(${APPLAUNCHER_PROJ}
  PRIVATE ep_boost::program_options

  vm_grpc_proto
  PRIVATE ${_REFLECTION}
  PRIVATE ${_GRPC_GRPCPP}
  PRIVATE ${_PROTOBUF_LIBPROTOBUF}
  -lrt
)
set_target_properties(${APPLAUNCHER_PROJ} PROPERTIES LINK_FLAGS_RELEASE -s)

install(TARGETS ${APPLAUNCHER_PROJ}
  RUNTIME DESTINATION bin
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
