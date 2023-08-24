#
# Copyright (c) 2022 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
#

cmake_minimum_required(VERSION 3.10)

set(CMAKE_GENERATOR "Unix Makefiles" CACHE INTERNAL "" FORCE)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
   SET(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(BoostCMake)
include(FTXUICMake)
include(gRPCFetchCMake)
include(revisionCMake)

set(PROTOS_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/services/protos/gens/)
include_directories(${PROTOS_GEN_DIR})
add_subdirectory(services/protos)

set(PROJECT_NAME "vm-manager")
project(${PROJECT_NAME})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unused-result -Wno-unused-variable -Wno-narrowing -O2")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g1 -O3 -fstack-protector-strong -Wdate-time -D_FORTIFY_SOURCE=2" CACHE STRING "CXX Release Flags" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O2 -g3")

find_package(PkgConfig REQUIRED)
include_directories(${DEPS_INCS_INCLUDE_DIRS})
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${EP_BOOST_INC_DIR})

file(GLOB guest_src guest/*.cc)
file(GLOB services_src services/*.cc)
file(GLOB utils_src utils/*.cc)

add_executable(${PROJECT_NAME} vm_manager.cc ${guest_src} ${services_src} ${utils_src})
add_dependencies(${PROJECT_NAME} vm_grpc_proto)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(${PROJECT_NAME}
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
  PRIVATE ep_boost::program_options
  PRIVATE ep_boost::log
  PRIVATE ep_boost::log_setup
  PRIVATE ep_boost::thread
  PRIVATE ep_boost::filesystem
  PRIVATE ep_boost::chrono

  vm_grpc_proto
  PRIVATE ${_REFLECTION}
  PRIVATE ${_GRPC_GRPCPP}
  PRIVATE ${_PROTOBUF_LIBPROTOBUF}
  -lrt
)
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS_RELEASE -s)

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
)

add_subdirectory(host/app_launcher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
