/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef SRC_GUEST_VSOCK_CID_POOL_H_
#define SRC_GUEST_VSOCK_CID_POOL_H_

#include <mutex>
#include <utility>
#include <bitset>

namespace vm_manager {

inline constexpr const uint32_t kCivMaxCidNum = 10000U;
inline constexpr const uint32_t kCiVCidStart = 1000U;

class VsockCidPool {
 public:
    uint32_t GetCid();

    uint32_t GetCid(uint32_t cid);

    bool ReleaseCid(uint32_t cid);

    static VsockCidPool &Pool(void);

 private:
    VsockCidPool() = default;
    ~VsockCidPool() = default;
    VsockCidPool(const VsockCidPool &) = delete;
    VsockCidPool& operator=(const VsockCidPool&) = delete;

    std::bitset<kCivMaxCidNum> bs_ = std::move(std::bitset<kCivMaxCidNum>{}.set());
    std::mutex mutex_;
};

}  // namespace vm_manager

#endif  // SRC_GUEST_VSOCK_CID_POOL_H_
