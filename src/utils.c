/*
 * Copyright (c) 2020 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "vm_manager.h"


int execute_cmd(const char *cmd, const char *arg, size_t arg_len, int daemonize)
{
	int pid;
	int wst;
	int i = 0;
	char *tok, *next_tok;
	char arg_dup[MAX_CMDLINE_LEN] = { 0 };
	char *argv[1024];
	char * arg_ptr = arg_dup;
	
	if (cmd == NULL) {
		fprintf(stderr, "%s: Invalid command\n", __func__);
		return -1;
	}

	if (arg_len > (sizeof(arg_dup) - 1)) {
		fprintf(stderr, "%s: argument execeed maxmium(%ld) length\n", __func__, sizeof(arg_dup));
		return -1;
	}

	argv[i++] = basename(strdup(cmd));

	strncpy(arg_dup, arg,sizeof(arg_dup) - 1);

	/* split cmd into argument array */
	while((tok = strtok_r(arg_ptr, " ", &next_tok))){
		argv[i++] = tok;
		arg_ptr = NULL;
	}
	argv[i] = NULL;

	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "%s: Failed to fork new process to execute %s\n", __func__, cmd);
		return -1;
	} else if (pid == 0) {
		execv(cmd, argv);
		return -1;
	} else {
		if (daemonize) {
			sleep(1);
			return 0;
		}

		wait(&wst);
		if (!(WIFEXITED(wst) && !WEXITSTATUS(wst))) {
			fprintf(stderr, "%s: Failed to execute %s %s\n", __func__, cmd, arg);
			return -1;
		}
	}

	return 0;
}

int cleanup_child_proc(void)
{
	int fd;
	int n;
	pid_t parent, child;
	char str[512] = { 0 };
	char buf[1024] = { 0 };
	char *tok, *ntok;
	int ret;

	parent = getpid();

	snprintf(str, 512, "/proc/%d/task/%d/children", parent, parent);

	/* Get child pid */
	fd = open(str, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "open %s failed, errno=%d\n", str, errno);
		return -1;
	}

	n = read(fd, buf, sizeof(buf) - 1);
	if (n == -1) {
		fprintf(stderr, "read %s failed, errno=%d\n", str, errno);
		close(fd);
		return -1;
	}
	close(fd);

	tok = strtok_r(buf, " ", &ntok);
	while (tok != NULL) {
		child = strtol(tok, NULL, 10);
		printf("terminate: pid=%d\n", child);
		ret = kill(child, SIGTERM);
		if (ret != 0)
			fprintf(stderr, "Failed to terminate sub-process: pid=%d, errno=%d\n", child, errno);
		tok = strtok_r(buf," ", &ntok);
	}

	return 0;
}
