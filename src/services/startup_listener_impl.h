/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef SRC_SERVICES_STARTUP_LISTENER_IMPL_H_
#define SRC_SERVICES_STARTUP_LISTENER_IMPL_H_

#include <map>
#include <memory>

#include "vm_host.grpc.pb.h"

namespace vm_manager {

class StartupListenerImpl final : public vm_manager::StartupListener::Service {
 public:
    StartupListenerImpl() = default;
    StartupListenerImpl(const StartupListenerImpl &) = delete;
    StartupListenerImpl& operator=(const StartupListenerImpl&) = delete;

    ~StartupListenerImpl() override = default;

    grpc::Status VmReady(grpc::ServerContext* ctx, const EmptyMessage* request, EmptyMessage* respond) override;

    void AddPendingVM(uint32_t cid, std::function<void(void)> callback);
    void RemovePendingVM(uint32_t cid);

 private:
    std::map<uint32_t, std::function<void(void)>> pending_vms_;
    std::mutex vm_lock_;
};

void RunListenerService(grpc::Service* listener,
                        const char *listener_address,
                        boost::latch *listener_ready,
                        std::shared_ptr<grpc::Server>* server_copy);


}  // namespace vm_manager

#endif  // SRC_SERVICES_STARTUP_LISTENER_IMPL_H_
