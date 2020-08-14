/*
 * Copyright (c) 2020 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __VTPM_H__
#define __VTPM_H__

#define SWTPM_SOCK "swtpm-sock"

int set_vtpm_bin_path(const char *bin_path);
int set_vtpm_data_dir(const char *data_dir);
int run_vtpm_daemon(void);

#endif /* __VTPM_H__ */
