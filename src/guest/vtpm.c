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
#include "vtpm.h"

struct _vtpm {
	char *bin;
	char *data_dir;
};

static struct _vtpm vtpm = { NULL, NULL };

int set_vtpm_bin_path(const char *bin_path)
{
	struct stat st;

	if (!bin_path)
		return -1;

	if (stat(bin_path, &st) != 0)
		return -1;

	vtpm.bin = strndup(bin_path, MAX_PATH);
	if (!vtpm.bin) {
		fprintf(stderr, "Failed to duplicate swtpm bin dir!\n");
		return -1;
	}

	return 0;
}

int set_vtpm_data_dir(const char *data_dir)
{
	struct stat st;

	if (!data_dir)
		return -1;

	if (stat(data_dir, &st) != 0)
		return -1;

	vtpm.data_dir = strndup(data_dir, MAX_PATH);
	if (!vtpm.data_dir) {
		fprintf(stderr, "Failed to duplicate swtpm data dir!\n");
		return -1;
	}

	return 0;
}

int run_vtpm_daemon(void)
{
	char cmd[1024] = { 0 };

	if (!vtpm.bin || !vtpm.data_dir) {
		fprintf(stderr, "Invalid vtpm params!\n");
		return -1;
	}

	memset(cmd, 0, sizeof(cmd));
	snprintf(cmd, 1024, "socket --tpmstate dir=%s --tpm2 --ctrl type=unixio,path=%s/%s", vtpm.data_dir,  vtpm.data_dir, SWTPM_SOCK);

	return execute_cmd(vtpm.bin, cmd, strlen(cmd), 1);
}
