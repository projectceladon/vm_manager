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

#include "vm_guest.grpc.pb.h"
#include "vm_manager.h"

class CivPowerCtlImpl final : public vm_manager::CivPowerCtl::Service {
 public:
    CivPowerCtlImpl() = default;
    CivPowerCtlImpl(const CivPowerCtlImpl &) = delete;
    CivPowerCtlImpl& operator=(const CivPowerCtlImpl&) = delete;

    ~CivPowerCtlImpl() override = default;

    grpc::Status Shutdown(grpc::ServerContext* ctx,
                          const vm_manager::EmptyMessage* request,
                          vm_manager::EmptyMessage* respond) override {
        std::cout << "Shutdown Signal from Host!" << std::endl;
        system("/system/bin/reboot -p shutdown");
        return grpc::Status::OK;
    }

 private:
};

int main() {
    char listener_address[50] = { 0 };
    snprintf(listener_address, sizeof(listener_address) - 1, "vsock:%u:%u", VMADDR_CID_ANY,
             vm_manager::kCivPowerCtlListenerPort);
    grpc::ServerBuilder builder;
    CivPowerCtlImpl listener;
    std::cout << "Civ Powerctl Listener listen@" << listener_address << std::endl;
    builder.AddListeningPort(listener_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&listener);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    if (server) {
        server->Wait();
    }
    return 0;
}
