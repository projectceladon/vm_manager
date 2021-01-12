/*
 * Copyright (c) 2020 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <sys/types.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "vm_manager.h"

static void get_result(int fd, char **result)
{
	int n;
	int buf_size = 256;
	int inc = 256;
	char *buf = (char *)calloc(buf_size, 1);
	assert(buf != NULL);

	while (1) {
		n = recv(fd, buf, buf_size - 1, MSG_PEEK);
		if (n == -1) {
			free(buf);
			fprintf(stderr, "%s: Failed to peek QMP output\n", __func__);
			return;
		}

		if (n == 0)
			break;

		if (buf[n - 1] == '\n')
			break;

		buf_size += inc;
		buf = realloc(buf, buf_size);
		assert(buf != NULL);
		memset(buf, 0, buf_size);
	}
	n = recv(fd, buf, buf_size - 1, 0);
	if (n == -1) {
		free(buf);
		fprintf(stderr, "%s: Failed to fetch QMP output\n", __func__);
		return;
	}

	if (result) {
		*result = buf;
	} else {
		free(buf);
	}
}

int send_qmp_cmd(const char *path, const char *cmd, char **result)
{
	int sock_fd;
	struct sockaddr_un sock;
	const char *qmp_cap_cmd = "{ \"execute\": \"qmp_capabilities\" }";
	int n;

	if (!path || !cmd) {
		fprintf(stderr, "%s: Invalid input param\n", __func__);
		return -1;
	}

	memset((char *)&sock, 0, sizeof(sock));
	sock.sun_family = AF_UNIX;
	strncpy(sock.sun_path, path, sizeof(sock.sun_path) - 1);

	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		warn("%s: Failed create socket: %s\n", __func__, strerror(errno));
		return -1;
	}

	if (connect(sock_fd, (struct sockaddr *)&sock, sizeof(struct sockaddr_un)) < 0) {
		warn("%s: Failed to connect %s(%d)", __func__, path, errno);
		close(sock_fd);
		return -1;
	}
	get_result(sock_fd, NULL);

	/* issue qmp_capability command */
	n = write(sock_fd, qmp_cap_cmd, strlen(qmp_cap_cmd));
	if (n < 0) {
		warn("%s: Failed to send cmd(%s) to socket: %s\n", __func__, qmp_cap_cmd, path);
		close(sock_fd);
		return -1;
	}
	get_result(sock_fd, NULL);

	/* issue qmp command */
	n = write(sock_fd, cmd, strlen(cmd));
	if (n < 0) {
		warn("%s: Failed to send cmd(%s) to socket: %s\n", __func__, cmd, path);
		close(sock_fd);
		return -1;
	}
	get_result(sock_fd, result);

	close(sock_fd);

	return 0;
}
