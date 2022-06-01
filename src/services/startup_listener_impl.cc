/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <map>

#include <boost/thread/latch.hpp>
#include <grpcpp/grpcpp.h>

#include "utils/log.h"
#include "services/startup_listener_impl.h"

namespace vm_manager {

grpc::Status StartupListenerImpl::VmReady(grpc::ServerContext* ctx,
                                          const EmptyMessage* request,
                                          EmptyMessage* respond) {
    uint32_t cid = 0;
    if (sscanf(ctx->peer().c_str(), "vsock:%u", &cid) != 1) {
        LOG(warning) << "Failed to parse peer vsock address " << ctx->peer();
        return grpc::Status(grpc::FAILED_PRECONDITION, "Invalid peer for StartupListener");
    }
    LOG(info) << "gRPC call from VM, cid=" << cid;

    std::scoped_lock lock(vm_lock_);
    auto iter = pending_vms_.find(cid);
    if (iter == pending_vms_.end()) {
        LOG(error) << "Received VmReady from VM with unknown cid: " << cid;
        return grpc::Status(grpc::FAILED_PRECONDITION, "VM is unknown");
    }

    if (iter->second != nullptr)
        iter->second();

    pending_vms_.erase(iter);

    return grpc::Status::OK;
}

void StartupListenerImpl::AddPendingVM(uint32_t cid, std::function<void(void)> callback) {
    std::scoped_lock lock(vm_lock_);

    pending_vms_[cid] = callback;
}

void StartupListenerImpl::RemovePendingVM(uint32_t cid) {
    std::scoped_lock lock(vm_lock_);

    pending_vms_.erase(cid);
}

void RunListenerService(grpc::Service* listener,
                        const char *listener_address,
                        boost::latch *listener_ready,
                        std::shared_ptr<grpc::Server>* server_copy) {
    grpc::ServerBuilder builder;
    LOG(info) << "Startup Listener listen@" << listener_address;
    builder.AddListeningPort(listener_address, grpc::InsecureServerCredentials());
    builder.RegisterService(listener);

    std::shared_ptr<grpc::Server> server(builder.BuildAndStart().release());

    *server_copy = server;
    listener_ready->try_count_down();

    if (server) {
        server->Wait();
    }
}

}  // namespace vm_manager
