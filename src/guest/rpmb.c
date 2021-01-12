/*
 * Copyright (c) 2020 Intel Corporation.
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
#include "rpmb.h"

struct _rpmb {
	char *bin;
	char *data_dir;
};

static struct _rpmb rpmb = { NULL, NULL };

int set_rpmb_bin_path(const char *bin_path)
{
	struct stat st;

	if (!bin_path) {
		fprintf(stderr, "%s: Invalid input!\n", __func__);
		return -1;
	}

	if (stat(bin_path, &st) != 0) {
		fprintf(stderr, "%s: cannot stat file: %s, errno=%d\n", __func__, bin_path, errno);
		return -1;
	}

	rpmb.bin = strndup(bin_path, MAX_PATH);
	if (!rpmb.bin) {
		fprintf(stderr, "Failed to duplicate rpmb bin path!\n");
		return -1;
	}

	return 0;
}

int set_rpmb_data_dir(const char *data_dir)
{
	struct stat st;

	if (!data_dir)
		return -1;

	if (stat(data_dir, &st) != 0)
		return -1;

	rpmb.data_dir = strndup(data_dir, MAX_PATH);
	if (!rpmb.data_dir) {
		fprintf(stderr, "Failed to duplicate rpmb data dir!\n");
		return -1;
	}

	return 0;
}

int run_rpmb_daemon(void)
{
	char path[1024] = { 0 };
	char cmd[1024] = { 0 };
	struct stat st;
	int ret;

	if (!rpmb.bin || !rpmb.data_dir)
		return -1;

	snprintf(path, 1024, "%s/%s", rpmb.data_dir, RPMB_DATA);

	if (stat(path, &st) != 0) {
		snprintf(cmd, 1024, "--dev %s/%s --init --size 2048", rpmb.data_dir, RPMB_DATA);
		ret = execute_cmd(rpmb.bin, cmd, strlen(cmd), 0);
		if (ret != 0)
			return -1;

		if (stat(path, &st) != 0)
			return -1;
	}

	memset(path, 0, sizeof(path));
	snprintf(path, 1024, "%s/%s", rpmb.data_dir, RPMB_SOCK);

	if (lstat(path, &st) == 0) {
		if (unlink(path) != 0)
			return -1;
	}

	memset(cmd, 0, sizeof(cmd));
	snprintf(cmd, 1024, "--dev %s/%s --sock %s/%s", rpmb.data_dir, RPMB_DATA, rpmb.data_dir, RPMB_SOCK);
	return execute_cmd(rpmb.bin, cmd, strlen(cmd), 1);
}

void cleanup_rpmb(void) {
	char path[1024] = { 0 };
	struct stat st;

	memset(path, 0, sizeof(path));
	snprintf(path, 1024, "%s/%s", rpmb.data_dir, RPMB_SOCK);

	if (stat(path, &st) == 0) {
		remove(path);
	}
}
