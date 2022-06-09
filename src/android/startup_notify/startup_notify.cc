/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <sys/socket.h>
#include <linux/vm_sockets.h>

#include <string>

#include <grpcpp/grpcpp.h>

#include "vm_host.grpc.pb.h"
#include "vm_manager.h"

class StartupNotify {
 public:
    explicit StartupNotify(std::shared_ptr<grpc::Channel> channel)
        : stub_(vm_manager::StartupListener::NewStub(channel)) {}

    bool VmReady(void) {
        vm_manager::EmptyMessage request;
        vm_manager::EmptyMessage reply;

        grpc::ClientContext context;

        grpc::Status status = stub_->VmReady(&context, request, &reply);

        if (status.ok()) {
            return true;
        } else {
            std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
            return false;
        }
    }

 private:
    std::unique_ptr<vm_manager::StartupListener::Stub> stub_;
};

int main() {
    char listener_address[50] = { 0 };
    snprintf(listener_address, sizeof(listener_address) - 1, "vsock:%u:%u",
                VMADDR_CID_HOST,
                vm_manager::kCivStartupListenerPort);
    StartupNotify notify(grpc::CreateChannel(listener_address, grpc::InsecureChannelCredentials()));
    if (!notify.VmReady()) {
        std::cerr << "Failed to notify host that civ is ready!" << std::endl;
        return -1;
    }
    return 0;
}
