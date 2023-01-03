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

#include <android/log.h>

#include <grpcpp/grpcpp.h>

#include "vm_guest.grpc.pb.h"
#include "vm_manager.h"

#define  LOG_TAG    "civ_applauncher"
#define  ALOG(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

class CivAppLauncherImpl final : public vm_manager::CivAppLauncher::Service {
 public:
    CivAppLauncherImpl() = default;
    CivAppLauncherImpl(const CivAppLauncherImpl &) = delete;
    CivAppLauncherImpl& operator=(const CivAppLauncherImpl&) = delete;

    ~CivAppLauncherImpl() override = default;

    grpc::Status LaunchApp(grpc::ServerContext* ctx,
                           const vm_manager::AppLaunchRequest* request,
                           vm_manager::AppLaunchResponse* respond) override {
        ALOG("Launch APP: %s, Disp id: %u\n", request->app_name().c_str(), request->disp_id());
        std::string cmd("/system/bin/am start -n " + request->app_name() +
                        " --display " + std::to_string(request->disp_id()));
        if (system(cmd.c_str())) {
            respond->set_code(-1);
            respond->set_status(vm_manager::AppStatus::FAILED);
        } else {
            respond->set_code(0);
            respond->set_status(vm_manager::AppStatus::LAUNCHED);
        }
        return grpc::Status::OK;
    }

 private:
};

int main() {
    char listener_address[50] = { 0 };
    snprintf(listener_address, sizeof(listener_address) - 1, "vsock:%u:%u", VMADDR_CID_ANY,
             vm_manager::kCivAppLauncherListenerPort);
    grpc::ServerBuilder builder;
    CivAppLauncherImpl listener;
    ALOG("Civ Applauncher Listener listen@%s\n", listener_address);
    builder.AddListeningPort(listener_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&listener);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    if (server) {
        server->Wait();
    }
    return 0;
}
