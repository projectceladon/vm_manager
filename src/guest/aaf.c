/*
 * Copyright (c) 2021 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "vm_manager.h"
#include "guest.h"
#include "utils.h"
#include "aaf.h"

#define AAF_FILE_NAME "mixins.spec"

static const char *gpu_type[] = {
	[AAF_GPU_TYPE_VIRTIO] = "gpu-type:virtio",
	[AAF_GPU_TYPE_GVTG]   = "gpu-type:gvtg",
	[AAF_GPU_TYPE_GVTD]   = "gpu-type:gvtd"
};

static const char *suspend_support[] = {
	[AAF_SUSPEND_DISABLE] = "suspend:false",
	[AAF_SUSPEND_ENABLE]  = "suspend:true"
};

static char *aaf_file = NULL;

static const char *aaf_config_array[AAF_CONFIG_NUM] = { NULL };

int set_aaf_path(const char *path)
{
	struct stat st;
	size_t len;

	if (!path) {
		fprintf(stderr, "%s: Invalid input!\n", __func__);
		return -1;
	}

	if (stat(path, &st) != 0) {
		fprintf(stderr, "%s: cannot stat folder: %s, errno=%d\n", __func__, path, errno);
		return -1;
	}

	len = strlen(path) + sizeof(AAF_FILE_NAME) + 1;

	aaf_file = calloc(len, 1);
	if (!aaf_file) {
		fprintf(stderr, "%s: Failed to alloc memory!\n", __func__);
		return -1;
	}

	snprintf(aaf_file, len, "%s/%s", path, AAF_FILE_NAME);

	return 0;
}

int flush_aaf_config(void)
{
	FILE *fp;
	int i;

	if (!aaf_file)
		return 0;

	fp = fopen(aaf_file, "w");
	if (!fp) {
		fprintf(stderr, "%s: Failed to open AAF file:%s\n", __func__, aaf_file);
		return -1;
	}

	for (i=0; i < AAF_CONFIG_NUM; i++) {
		if (aaf_config_array[i])
			fprintf(fp, "%s\n", aaf_config_array[i]);
	}

	fclose(fp);

	return 0;
}

int set_aaf_option(aaf_config_opt_t opt, unsigned int sub)
{
	if (!aaf_file)
		return 0;

	switch (opt) {
		case AAF_CONFIG_SUSPEND:
			if (sub > AAF_SUSPEND_DISABLE)
				return -1;
			aaf_config_array[AAF_CONFIG_SUSPEND] = suspend_support[sub];
			break;
		case AAF_CONFIG_GPU_TYPE:
			if (sub > AAF_GPU_TYPE_GVTD)
				return -1;
			aaf_config_array[AAF_CONFIG_GPU_TYPE] = gpu_type[sub];
			break;
		default:
			fprintf(stderr, "%s: Invalid AAF config option!\n", __func__);
			break;
	}

	return 0;
}
