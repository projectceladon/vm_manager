#
# Copyright (c) 2022 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
#

cmake_minimum_required(VERSION 3.10)

#SET(MAJOR_VERSION 1)
#SET(MINOR_VERSION 0)
#SET(PATCH_VERSION 0)

#SET(CPACK_GENERATOR "DEB")

#SET(CPACK_PACKAGE_VERSION_MAJOR ${MAJOR_VERSION})
#SET(CPACK_PACKAGE_VERSION_MINOR ${MINOR_VERSION})
#SET(CPACK_PACKAGE_VERSION_PATCH ${PATCH_VERSION})

#SET(CPACK_PACKAGE_NAME "vm-manager")
#SET(CPACK_DEBIAN_PACKAGE_NAME "vm-manager")
#SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
#SET(CPACK_DEBIAN_PACKAGE_DESCRIPTION "CiV VM Manager to manage the CiV guests")
#SET(CPACK_PACKAGE_CONTACT "yadong.qi@intel.com")
#SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Qi, Yadong <yadong.qi@intel.com>")

project("vm_manager")
add_subdirectory(src)
