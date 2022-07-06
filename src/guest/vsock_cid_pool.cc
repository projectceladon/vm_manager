/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <guest/vsock_cid_pool.h>

namespace vm_manager {

uint32_t VsockCidPool::GetCid() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t pos = bs_._Find_next(kCiVCidStart - 1);
    if (pos == bs_.size())
        return 0;
    bs_.reset(pos);
    return pos;
}

uint32_t VsockCidPool::GetCid(uint32_t cid) {
    std::lock_guard<std::mutex> lock(mutex_);
    if ((cid < 3) || (cid >= kCivMaxCidNum))
        return 0;
    if (!bs_[cid])
        return 0;

    bs_.reset(cid);
    return cid;
}

bool VsockCidPool::ReleaseCid(uint32_t cid) {
    if ((cid < 3) || (cid >= kCivMaxCidNum))
        return false;
    std::lock_guard<std::mutex> lock(mutex_);
    bs_.set(cid);
    return true;
}

VsockCidPool &VsockCidPool::Pool(void) {
    static VsockCidPool vcp_;
    return vcp_;
}

}  // namespace vm_manager
