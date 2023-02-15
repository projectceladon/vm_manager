/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef SRC_GUEST_VM_BUILDER_H_
#define SRC_GUEST_VM_BUILDER_H_

#include <string>
#include <vector>
#include <utility>
#include <exception>
#include <map>

#include <boost/thread.hpp>
#include <boost/process.hpp>

#include "guest/config_parser.h"
#include "guest/vm_process.h"
#include "utils/log.h"

namespace vm_manager {

class VmBuilder {
 public:
    enum VmState {
        kVmEmpty = 0,
        kVmCreated,
        kVmBooting,
        kVmRunning,
        kVmPaused,
        kVmUnknown,
    };

 public:
    explicit VmBuilder(std::string name) : name_(name), vsock_cid_(0) {}
    virtual ~VmBuilder() = default;
    virtual bool BuildVmArgs(void) = 0;
    virtual void StartVm(void) = 0;
    virtual void WaitVmExit(void) = 0;
    virtual void StopVm(void) = 0;
    virtual void PauseVm(void) = 0;
    virtual bool WaitVmReady(void) = 0;
    virtual void SetVmReady(void) = 0;
    virtual void SetProcessEnv(std::vector<std::string> env) = 0;
    std::string GetName(void);
    uint32_t GetCid(void);
    VmState GetState(void);

 protected:
    std::string name_;
    uint32_t vsock_cid_;
    VmState state_ = VmBuilder::VmState::kVmEmpty;
    std::mutex state_lock_;
};

static inline constexpr const char *VmStateToStr(VmBuilder::VmState s) {
    switch (s) {
        case VmBuilder::kVmEmpty:   return "Empty";
        case VmBuilder::kVmCreated: return "Created";
        case VmBuilder::kVmBooting: return "Booting";
        case VmBuilder::kVmRunning: return "Running";
        case VmBuilder::kVmPaused:  return "Paused";
        case VmBuilder::kVmUnknown: return "Unknown";
    }
    return "NaN";
}


}  // namespace vm_manager

#endif  // SRC_GUEST_VM_BUILDER_H_
