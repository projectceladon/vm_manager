/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef SRC_GUEST_VM_FLASH_H_
#define SRC_GUEST_VM_FLASH_H_

#include <string>

#include <guest/config_parser.h>

namespace vm_manager {

class VmFlasher final {
 public:
    VmFlasher() = default;
    ~VmFlasher() = default;
    bool FlashGuest(std::string path);

 private:
    bool QemuCreateVirtUsbDisk(void);
    bool QemuCreateVirtualDisk(void);
    bool FlashWithQemu(void);
    bool CheckImages(boost::filesystem::path o_dir);

 private:
    size_t total_image_size_ = 0;
    std::string virtual_disk_;
    CivConfig cfg_;
};

}  // namespace vm_manager

#endif  // SRC_GUEST_VM_FLASH_H_
