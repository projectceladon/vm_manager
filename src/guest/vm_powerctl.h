
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef SRC_GUEST_VM_POWERCTL_H_
#define SRC_GUEST_VM_POWERCTL_H_

#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include <grpcpp/grpcpp.h>

#include "vm_guest.grpc.pb.h"
#include "include/constants/vm_manager.h"

namespace vm_manager {

class CivVmPowerCtl {
 public:
    explicit CivVmPowerCtl(std::shared_ptr<grpc::Channel> channel)
        : stub_(vm_manager::CivPowerCtl::NewStub(channel)) {}

    bool Shutdown(void) {
        vm_manager::EmptyMessage request;
        vm_manager::EmptyMessage reply;

        grpc::ClientContext context;

        grpc::Status status = stub_->Shutdown(&context, request, &reply);

        if (status.ok()) {
            return true;
        } else {
            std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
            return false;
        }
    }

 private:
    std::unique_ptr<vm_manager::CivPowerCtl::Stub> stub_;
};


}  // namespace vm_manager

#endif  // SRC_GUEST_VM_POWERCTL_H_
