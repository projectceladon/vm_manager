
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <utility>

#include <boost/process.hpp>

#include "guest/vm_builder.h"
#include "guest/config_parser.h"
#include "services/message.h"
#include "utils/log.h"
#include "utils/utils.h"

namespace vm_manager {
    std::string VmBuilder::GetName(void) {
        return name_;
    }

    uint32_t VmBuilder::GetCid(void) {
        return vsock_cid_;
    }

    VmBuilder::VmState VmBuilder::GetState(void) {
        return state_;
    }
}  //  namespace vm_manager
