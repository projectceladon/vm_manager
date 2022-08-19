#
# Copyright (c) 2022 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
#
include(ExternalProject)

set(DEPENDENCIES)
set(EXTRA_CMAKE_ARGS)

set(BOOST_BOOTSTRAP_COMMAND ./bootstrap.sh)
set(BOOST_BUILD_TOOL ./b2)
set(BOOST_CXXFLAGS "cxxflags=-std=c++17")
set(BOOST_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/INSTALL/boost_1.79.0)

ExternalProject_Add(ep_boost

    PREFIX ${CMAKE_BINARY_DIR}/external
    TIMEOUT 300

    URL https://boostorg.jfrog.io/artifactory/main/release/1.79.0/source/boost_1_79_0.tar.bz2

    URL_HASH SHA256=475d589d51a7f8b3ba2ba4eda022b170e562ca3b760ee922c146b6c65856ef39

    BUILD_IN_SOURCE 1

    CONFIGURE_COMMAND ${BOOST_BOOTSTRAP_COMMAND}

    BUILD_ALWAYS 0
    BUILD_COMMAND ./b2 install
                  ${BOOST_CXXFLAGS}
                  threading=multi
                  variant=release
                  link=static
                  --prefix=${BOOST_INSTALL_PREFIX}
                  --layout=system
                  --with-program_options
                  --with-log
                  --with-filesystem
                  --with-chrono

    INSTALL_COMMAND ""
    LOG_CONFIGURE 1
    LOG_BUILD 1
    LOG_INSTALL 1
)

set(EP_BOOST_INC_DIR ${BOOST_INSTALL_PREFIX}/include)
set(EP_BOOST_LIB_DIR ${BOOST_INSTALL_PREFIX}/lib)

function(add_boost_lib)
  set(mod_name ${ARGV0})

  add_library(ep_boost::${mod_name} STATIC IMPORTED GLOBAL)
  set_target_properties(
    ep_boost::${mod_name} PROPERTIES
    IMPORTED_LOCATION ${EP_BOOST_LIB_DIR}/libboost_${mod_name}.a
  )
  add_dependencies(ep_boost::${mod_name} ep_boost)
endfunction()

add_boost_lib(thread)
add_boost_lib(program_options)
add_boost_lib(log)
add_boost_lib(log_setup)
add_boost_lib(filesystem)
add_boost_lib(chrono)
