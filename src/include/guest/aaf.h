/*
 * Copyright (c) 2020 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __AAF_H__
#define __AAF_H__

typedef enum {
    AAF_CONFIG_GPU_TYPE = 0,
    AAF_CONFIG_SUSPEND,
    AAF_CONFIG_NUM
} aaf_config_opt_t;

enum {
    AAF_SUSPEND_DISABLE = 0,
    AAF_SUSPEND_ENABLE
};

enum {
    AAF_GPU_TYPE_VIRTIO = 0,
    AAF_GPU_TYPE_GVTG,
    AAF_GPU_TYPE_GVTD
};

int set_aaf_path(const char *bin_path);
int set_aaf_option(aaf_config_opt_t opt, unsigned int sub);
int flush_aaf_config(void);

#endif /* __AAF_H__ */