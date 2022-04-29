/*
 * Copyright (c) 2020 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <string.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "vm_manager.h"
#include "guest.h"
#include "utils.h"
#include "rpmb.h"
#include "vtpm.h"

#define MEGABYTE *1024L*1024L
#define GIGABYTE *1024L*1024L*1024L

#define DD_BS (63 MEGABYTE)

extern keyfile_group_t g_group[];

#define VUSB_FLASH_DISK "/tmp/flash.vfat"

static char flashing_arg[] =
	" -name civ_flashing"
	" -M q35"
	" -m 2048M"
	" -smp 1"
	" -enable-kvm"
	" -k en-us"
	" -device qemu-xhci,id=xhci,addr=0x5"
	" -drive file="VUSB_FLASH_DISK",id=udisk1,format=raw,if=none"
	" -device usb-storage,drive=udisk1,bus=xhci.0"
	" -no-reboot"
	" -nographic -display none -serial mon:stdio"
	" -boot menu=on,splash-time=5000,strict=on ";

static size_t total_images_size = 0; // total size in Bytes

static int split_large_image(const char *fpath, const struct stat *st, int tflag)
{
        char cmd[MAX_CMDLINE_LEN] = { 0 };

        if (tflag != FTW_F)
                return 0;

	total_images_size += st->st_size;
        if (st->st_size < (4 GIGABYTE)) {
                return 0;
	}

        snprintf(cmd, 1024, "split --bytes=%ld --numeric-suffixes %s %s.part", (4 GIGABYTE) -  1, fpath, fpath);
        printf("%s\n", cmd);
        if(system(cmd) != 0) {
                return -1;
        }
        remove(fpath);
        return 0;
}

static int create_vusb(GKeyFile *gkf)
{
	char *p = NULL;
	char cmd[MAX_CMDLINE_LEN] = { 0 };
	char buf[PATH_MAX] = { 0 };
	char *fname = NULL;
	keyfile_group_t *g = NULL;
	g_autofree gchar *file = NULL;

	g = &g_group[GROUP_GLOB];
	file = g_key_file_get_string(gkf, g->name, g->key[GLOB_FLASHFILES], NULL);
	if (file == NULL) {
		g_warning("cannot find key flashfiles from group global\n");
		return -1;
	}

	fname = basename(strdup(file));
	p = strstr(fname, ".zip");
	if (p) {
		memset(p, '\0', 4);
	} else {
		fprintf(stderr, "Invalid flashfiles: %s\n", file);
		return -1;
	}

	snprintf(cmd, MAX_CMDLINE_LEN, "rm -rf /tmp/%s", fname);
	printf("%s\n", cmd);
	if (system(cmd))
		fprintf(stderr, "Warning: Failed to remove old tmp files: /tmp/%s\n", fname);

	snprintf(cmd, MAX_CMDLINE_LEN, "unzip -o %s -d /tmp/%s", file, fname);
	printf("%s\n", cmd);
	if (system(cmd))
		return -1;

	snprintf(buf, PATH_MAX, "/tmp/%s", fname);
	if (ftw(buf, split_large_image, 10) == -1) {
		fprintf(stderr, "failed to tree walk");
		return -1;
	}

	snprintf(cmd, MAX_CMDLINE_LEN, "dd if=/dev/zero of="VUSB_FLASH_DISK" bs=%ld count=%ld",
					DD_BS, (total_images_size + DD_BS - 1)/DD_BS);
	printf("%s\n", cmd);
	if (system(cmd))
		return -1;

	memset(cmd, 0, sizeof(cmd));
	snprintf(cmd, MAX_CMDLINE_LEN, "mkfs.vfat /tmp/flash.vfat");
	printf("%s\n", cmd);
	if (system(cmd))
		return -1;

	snprintf(cmd, MAX_CMDLINE_LEN, "mcopy -o -n -i /tmp/flash.vfat /tmp/%s/* ::", fname);
	printf("%s\n", cmd);
	if (system(cmd))
		return -1;

	memset(cmd, 0, sizeof(cmd));
	snprintf(cmd, MAX_CMDLINE_LEN, "rm -r /tmp/%s", fname);
	if (system(cmd))
		return -1;

	return 0;
}

int create_vdisk(GKeyFile *gkf)
{
	keyfile_group_t *g = NULL;
	g_autofree gchar *path = NULL, *size = NULL;
	char cmd[MAX_CMDLINE_LEN] = { 0 };

	g = &g_group[GROUP_DISK];

	size = g_key_file_get_string(gkf, g->name, g->key[DISK_SIZE], NULL);
	if (size == NULL) {
		g_warning("cannot find key disk size from group disk\n");
		return -1;
	}

	path = g_key_file_get_string(gkf, g->name, g->key[DISK_PATH], NULL);
	if (path == NULL) {
		g_warning("cannot find key disk path from group disk\n");
		return -1;
	}

	snprintf(cmd, 1024, "qemu-img create -f qcow2 %s %s", path, size);

	printf("%s\n", cmd);
	return system(cmd);
}

int flash_guest(char *name)
{
	int cx;
	int ret = -1;
	GKeyFile *gkf;
	g_autofree gchar *val = NULL;
	char cfg_file[1024] = {0};
	char cmd[MAX_CMDLINE_LEN] = {0};
	char *p = &cmd[0];
	keyfile_group_t *g = NULL;

	if (!name) {
		fprintf(stderr, "%s: Invalid input param\n", __func__);
		return -1;
	}

	snprintf(cfg_file, 1024, "%s/%s.ini", civ_config_path, name);

	gkf = g_key_file_new();

	if (!g_key_file_load_from_file(gkf, cfg_file, G_KEY_FILE_NONE, NULL)) {
		g_warning("Error loading ini file :%s", cfg_file);
		goto exit;
	}

	if (create_vusb(gkf)) {
		fprintf(stderr, "Failed to create virtual USB");
		goto exit;
	}

	if (create_vdisk(gkf)) {
		fprintf(stderr, "Failed to create virtual Disk");
		goto exit;
	}

	g = &g_group[GROUP_EMUL];
	val = g_key_file_get_string(gkf, g->name, g->key[EMUL_PATH], NULL);
	if (val == NULL) {
		g_warning("cannot find key emu_path from group general\n");
		goto exit;
	}
	cx = snprintf(p, 100, "%s", val);
	p += cx;

	g = &g_group[GROUP_RPMB];
	val = g_key_file_get_string(gkf, g->name, g->key[RPMB_BIN_PATH], NULL);
	if (val != NULL) {
		if (0 == set_rpmb_bin_path(val)) {
			val = g_key_file_get_string(gkf, g->name, g->key[RPMB_DATA_DIR], NULL);
			if (val != NULL) {
				if (0 == set_rpmb_data_dir(val)) {
					cx = snprintf(p, 200, " -device virtio-serial -device virtserialport,chardev=rpmb0,name=rpmb0 -chardev socket,id=rpmb0,path=%s/%s",
							val, RPMB_SOCK);
					p += cx;
				}
			}
		}
	}

	g = &g_group[GROUP_VTPM];
	val = g_key_file_get_string(gkf, g->name, g->key[VTPM_BIN_PATH], NULL);
	if (val != NULL) {
		if (0 == set_vtpm_bin_path(val)) {
			val = g_key_file_get_string(gkf, g->name, g->key[VTPM_DATA_DIR], NULL);
			if (val != NULL) {
				if (0 == set_vtpm_data_dir(val)) {
					cx = snprintf(p, 200, " -chardev socket,id=chrtpm,path=%s/%s -tpmdev emulator,id=tpm0,chardev=chrtpm -device tpm-crb,tpmdev=tpm0", val, SWTPM_SOCK);
					p += cx;
				}
			}
		}
	}

	g = &g_group[GROUP_FIRM];
	val = g_key_file_get_string(gkf, g->name, g->key[FIRM_TYPE], NULL);
	if (val == NULL) {
		g_warning("cannot find key name from group firmware\n");
		goto exit;
	}
	if (strcmp(val, FIRM_OPTS_UNIFIED_STR) == 0) {
		val = g_key_file_get_string(gkf, g->name, g->key[FIRM_PATH], NULL);
		cx = snprintf(p, 100, " -drive if=pflash,format=raw,file=%s", val);
		p += cx;;
	} else if (strcmp(val, FIRM_OPTS_SPLITED_STR) == 0) {
		val = g_key_file_get_string(gkf, g->name, g->key[FIRM_CODE], NULL);
		cx = snprintf(p, 100, " -drive if=pflash,format=raw,readonly,file=%s", val);
		p += cx;
		val = g_key_file_get_string(gkf, g->name, g->key[FIRM_VARS], NULL);
		cx = snprintf(p, 100, " -drive if=pflash,format=raw,file=%s", val);
		p += cx;
	} else {
		g_warning("cannot find firmware sub-key\n");
		return -1;
	}

	g = &g_group[GROUP_DISK];
	val = g_key_file_get_string(gkf, g->name, g->key[DISK_PATH], NULL);
	if (val == NULL) {
		g_warning("cannot find key disk path from group disk\n");
		goto exit;
	}
	cx = snprintf(p, 1024, " -device virtio-scsi-pci,id=scsi0,addr=0x8"
			       " -drive file=%s,if=none,format=qcow2,id=scsidisk1"
			       " -device scsi-hd,drive=scsidisk1,bus=scsi0.0", val);
	p += cx;

	cx = snprintf(p, sizeof(flashing_arg), "%s", flashing_arg);
	p += cx;

	run_vtpm_daemon();

	run_rpmb_daemon();

	printf("%s\n", cmd);

	ret = system(cmd);
	if (ret == 0)
		printf("\nFlash guest[%s] done\n", name);
	else
		printf("\nFlash guest[%s] failed\n", name);

exit:
	//Clean up temp file
	memset(cmd, 0, sizeof(cmd));
	snprintf(cmd, MAX_CMDLINE_LEN, "rm -f %s", VUSB_FLASH_DISK);
	ret = system(cmd);

	cleanup_child_proc();
	g_key_file_free(gkf);
	return ret;
}
