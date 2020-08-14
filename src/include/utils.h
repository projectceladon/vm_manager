/*
 * Copyright (c) 2020 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __UTILS_H__
#define __UTILS_H__

int execute_cmd(const char *cmd, const char *arg, size_t arg_len, int daemonize);

int cleanup_child_proc(void);

#endif /* __UTILS_H__ */
