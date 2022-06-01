/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef SRC_GUEST_AAF_H_
#define SRC_GUEST_AAF_H_

#include <string>
#include <map>
#include <utility>

namespace vm_manager {

extern const char *kAafKeyGpuType;
extern const char *kAafKeySuspend;
extern const char *kAafKeyAudioType;

class Aaf {
 public:
    explicit Aaf(const char *path);

    void Set(std::string k, std::string v);

    void Flush(void);

 private:
    Aaf(const Aaf&) = delete;
    Aaf& operator=(const Aaf&) = delete;

    std::string path_;
    std::map<std::string, std::string> data_;
};

}  //  namespace vm_manager

#endif  // SRC_GUEST_AAF_H_
