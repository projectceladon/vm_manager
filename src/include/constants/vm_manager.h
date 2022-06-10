/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef SRC_INCLUDE_CONSTANTS_VM_MANAGER_H_
#define SRC_INCLUDE_CONSTANTS_VM_MANAGER_H_
namespace vm_manager {
    inline constexpr int kCivStartupListenerPort = 9999U;
    inline constexpr int kCivPowerCtlListenerPort = 10000U;
    inline constexpr int kCivAppLauncherListenerPort = 10001U;
}  // namespace vm_manager

#endif  // SRC_INCLUDE_CONSTANTS_VM_MANAGER_H_
