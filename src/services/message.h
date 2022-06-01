/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef SRC_SERVICES_MESSAGE_H_
#define SRC_SERVICES_MESSAGE_H_

#include <string>

#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/string.hpp>

namespace vm_manager {

typedef boost::interprocess::allocator<char, boost::interprocess::managed_shared_memory::segment_manager> CharAllocator;
typedef boost::interprocess::basic_string<char, std::char_traits<char>, CharAllocator> bstring;

inline constexpr const char *kCivServerMemName = "CivServerShm";
inline constexpr const char *kCivServerObjSync = "Civ Message Sync";
inline constexpr const char *kCivServerObjData = "Civ Message Data";


enum CivMsgType {
    kCiVMsgStopServer = 100U,
    kCivMsgListVm,
    kCivMsgImportVm,
    kCivMsgStartVm,
    kCivMsgStopVm,
    kCivMsgTest,
    kCivMsgRespondSuccess = 500U,
    kCivMsgRespondFail,
};

struct CivMsgSync {
    boost::interprocess::interprocess_mutex mutex;
    boost::interprocess::interprocess_mutex mutex_cond;
    boost::interprocess::interprocess_condition cond_s;
    boost::interprocess::interprocess_condition cond_c;
};

struct CivMsg {
    enum { MaxPayloadSize = 1024U };

    CivMsgType type;
    char payload[MaxPayloadSize];
};

}  // namespace vm_manager

#endif  // SRC_SERVICES_MESSAGE_H_
