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

typedef struct
{
	FILE *read_fd;
	FILE *write_fd;
	pid_t child_pid;
} SPC_PIPE;

int write_to_file(const char *path, const char *buffer);
int find_pci(const char * name, int n, char *res[]);
int find_ProgIf(char *pci_device, char * res); 
int load_kernel_module(const char *module);

#endif /* __UTILS_H__ */
