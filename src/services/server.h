/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef SRC_SERVICES_SERVER_H_
#define SRC_SERVICES_SERVER_H_

#include <string>
#include <vector>
#include <memory>

#include <boost/thread/latch.hpp>

#include "utils/log.h"
#include "guest/vm_builder.h"
#include "services/message.h"
#include "services/startup_listener_impl.h"

namespace vm_manager {

struct StartupListenerInst {
    StartupListenerImpl listener;
    std::unique_ptr<boost::thread> thread;
    std::shared_ptr<grpc::Server> server;
};

class Server final {
 public:
    static Server &Get(void);
    void Start(void);
    void Stop(void);

 private:
    Server() = default;
    ~Server() = default;

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    size_t FindVmInstance(std::string name);
    void DeleteVmInstance(std::string name);

    int ListVm(const char payload[]);
    int ImportVm(const char payload[]);
    int StartVm(const char payload[]);
    int StopVm(const char payload[]);
    int GetVmInfo(const char payload[]);

    void VmThread(VmBuilder *vb, boost::latch *wait_continue);

    void Accept();

    void AsyncWaitSignal(void);

    bool SetupStartupListenerService();

    bool stop_server_ = false;

    CivMsgSync *sync_ = nullptr;

    std::vector<std::unique_ptr<VmBuilder>> vmis_;

    std::mutex vmis_mutex_;

    StartupListenerInst startup_listener_;
};

}  // namespace vm_manager

#endif  // SRC_SERVICES_SERVER_H_
