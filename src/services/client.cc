/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <string>
#include <vector>
#include <utility>
#include <cassert>

#include <boost/process/environment.hpp>

#include "services/client.h"
#include "services/message.h"
#include "services/server.h"
#include "utils/log.h"

namespace vm_manager {

std::vector<std::string> Client::GetGuestLists(void) {
    std::vector<std::string> vms;
    std::pair<bstring *, int> vm_lists = client_shm_.find<bstring>("ListVms");
    for (auto i = 0; i < vm_lists.second; i++) {
        vms.push_back(vm_lists.first[i].c_str());
    }
    return vms;
}

void Client::PrepareStartGuestClientShm(const char *path) {
    boost::process::environment env = boost::this_process::environment();

    client_shm_.destroy<bstring>("StartVmCfgPath");
    client_shm_.destroy<bstring>("StartVmEnv");
    client_shm_.zero_free_memory();

    bstring *var_name = client_shm_.construct<bstring>
                ("StartVmCfgPath")
                (path, client_shm_.get_segment_manager());

    bstring *var_env = client_shm_.construct<bstring>
                ("StartVmEnv")
                [env.size()]
                (client_shm_.get_segment_manager());

    if (!var_env)
        return;

    for (std::string s : env._data) {
        var_env->assign(s.c_str());
        var_env++;
    }
}

void Client::PrepareStopGuestClientShm(const char *vm_name) {
    client_shm_.destroy<bstring>("StopVmName");
    client_shm_.zero_free_memory();

    bstring *var_name = client_shm_.construct<bstring>
                ("StopVmName")
                (vm_name, client_shm_.get_segment_manager());
}

void Client::PrepareGetGuestInfoClientShm(const char *vm_name) {
    client_shm_.destroy<bstring>("VmName");
    client_shm_.zero_free_memory();

    bstring *var_name = client_shm_.construct<bstring>
                ("VmName")
                (vm_name, client_shm_.get_segment_manager());
}

CivVmInfo Client::GetCivVmInfo(const char *vm_name) {
    PrepareGetGuestInfoClientShm(vm_name);
    Notify(kCivMsgGetVmInfo);
    CivVmInfo *vm_info = client_shm_.find_or_construct<CivVmInfo>
                ("VmInfo")
                (0, VmBuilder::VmState::kVmUnknown);
    assert(vm_info != nullptr);
    return std::move(*vm_info);
}

bool Client::Notify(CivMsgType t) {
    std::pair<CivMsgSync*, boost::interprocess::managed_shared_memory::size_type> sync;
    sync = server_shm_.find<CivMsgSync>(kCivServerObjSync);
    if (!sync.first || (sync.second != 1)) {
        LOG(error) << "Failed to find sync block!" << sync.first << " size=" << sync.second;
        return false;
    }
    boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock_clients(sync.first->mutex);

    CivMsg *data = server_shm_.construct<CivMsg>
                (kCivServerObjData)
                ();

    if (!data) {
        return false;
    }
    data->type = t;
    strncpy(data->payload, client_shm_name_.c_str(), sizeof(data->payload) - 1);
    data->payload[sizeof(data->payload) - 1] = '\0';

    boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock_cond(sync.first->mutex_cond);
    sync.first->cond_s.notify_one();
    sync.first->msg_in = true;

    int retry_cnt = 0;
    constexpr const int kMaxRetryCount = 100;
    while (sync.first->msg_in && (retry_cnt < kMaxRetryCount)) {
        boost::interprocess::cv_status cs = sync.first->cond_c.wait_for(lock_cond, boost::chrono::seconds(1));
        if (cs != boost::interprocess::cv_status::timeout) {
            break;
        } else {
            retry_cnt++;
        }
    }

    int ret = false;
    if (retry_cnt < kMaxRetryCount)
        ret = data->type == kCivMsgRespondSuccess ? true : false;
    else
        LOG(error) << "Server is not responding!";

    server_shm_.destroy<CivMsg>(kCivServerObjData);
    server_shm_.zero_free_memory();

    return ret;
}

Client::Client() {
    server_shm_ = boost::interprocess::managed_shared_memory(boost::interprocess::open_only, kCivServerMemName);

    client_shm_name_ = std::string("CivClientShm" + std::to_string(getpid()));
    boost::interprocess::permissions perm;
    perm.set_unrestricted();
    client_shm_ = boost::interprocess::managed_shared_memory(
            boost::interprocess::create_only,
            client_shm_name_.c_str(),
            65536,
            0,
            perm);
}

Client::~Client() {
    boost::interprocess::shared_memory_object::remove(client_shm_name_.c_str());
}

}  // namespace vm_manager
