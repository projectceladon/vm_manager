/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef SRC_SERVICES_CLIENT_H_
#define SRC_SERVICES_CLIENT_H_

#include <string>
#include <vector>

#include <boost/interprocess/managed_shared_memory.hpp>

#include "services/message.h"

namespace vm_manager {
class Client {
 public:
    Client();
    ~Client();

    void PrepareImportGuestClientShm(const char *cfg_path);
    std::vector<std::string> GetGuestLists(void);
    void PrepareStartGuestClientShm(const char *vm_name);
    void PrepareStopGuestClientShm(const char *vm_name);
    void PrepareGetGuestInfoClientShm(const char *vm_name);
    CivVmInfo GetCivVmInfo(const char *vm_name);
    bool Notify(CivMsgType t);

 private:
    boost::interprocess::managed_shared_memory server_shm_;
    boost::interprocess::managed_shared_memory client_shm_;
    std::string client_shm_name_;
};

}  // namespace vm_manager

#endif  // SRC_SERVICES_CLIENT_H_
