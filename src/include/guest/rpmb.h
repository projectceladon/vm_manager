/*
 * Copyright (c) 2020 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __RPMB_H__
#define __RPMB_H__

#define RPMB_DATA "RPMB_DATA"
#define RPMB_SOCK "rpmb-sock"

int set_rpmb_bin_path(const char *bin_path);
int set_rpmb_data_dir(const char *data_dir);
int run_rpmb_daemon(void);
void cleanup_rpmb(void);

#endif /* __RPMB_H__ */
