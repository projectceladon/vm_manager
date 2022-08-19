/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef SRC_UTILS_UTILS_H_
#define SRC_UTILS_UTILS_H_

#ifndef MAX_PATH
#define MAX_PATH 2048U
#endif

#define CIV_GUEST_QMP_SUFFIX     ".qmp.unix.socket"

const char *GetConfigPath(void);
int Daemonize(void);

constexpr std::size_t operator""_KB(unsigned long long v) {
    return 1024u * v;
}

constexpr std::size_t operator""_MB(unsigned long long v) {
    return 1024u * 1024u * v;
}

constexpr std::size_t operator""_GB(unsigned long long v) {
    return 1024u * 1024u * 1024u * v;
}


#endif  // SRC_UTILS_UTILS_H_
