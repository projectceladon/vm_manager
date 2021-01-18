/*
 * Copyright (c) 2020 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __VM_MANAGER_H__
#define __VM_MANAGER_H__

#define CIV_GUEST_QMP_SUFFIX     ".qmp.unix.socket"
#define MAX_CMDLINE_LEN 4096U

#ifndef MAX_PATH
#define MAX_PATH 2048U
#endif

#define str(s) #s
#define xstr(s) str(s)

extern char civ_config_path[];

int create_guest(char *name);
int import_guest(char *path);
int start_guest(char *name);
int flash_guest(char *name);
void list_guests(void);
int stop_guest(char *name);
int delete_guest(char *name);

void set_cleanup();

#endif /* __VM_MANAGER_H__*/
