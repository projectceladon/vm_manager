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
#include <glib.h>
#include <signal.h>
#include <assert.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <errno.h>
#include <json.h>
#include <uuid/uuid.h>
#include <sys/stat.h>
#include "vm_manager.h"
#include "guest.h"

keyfile_group_t g_group[] = {
	{ "global",   { "name", "flashfiles", "adb_port", "fastboot_port", NULL } },
	{ "emulator", { "path", NULL } },
	{ "memory",   { "size", NULL } },
	{ "vcpu",     { "num",  NULL } },
	{ "firmware", { "type", "path", "code", "vars", NULL } },
	{ "disk",     { "path", "size", NULL } },
	{ "graphics", { "type", "gvtg_version", "vgpu_uuid", NULL } },
	{ "vtpm",     { "bin_path", "data_dir", NULL } },
	{ "rpmb",     { "bin_path", "data_dir", NULL } },
	{ "aaf",      { "path", "support_suspend", NULL } },
	{ "extra",    { "cmd", NULL } },
	{ "passthrough", { "passthrough_pci", NULL}},
};

int load_form_data(char *name)
{
	GKeyFile *in = NULL;
	char file[MAX_PATH] = { 0 };
	g_autofree gchar *val = NULL;
	keyfile_group_t *g = NULL;

	if (!name)
		return -1;

	snprintf(file, sizeof(file) - 1, "%s/%s.ini", civ_config_path, name);
	in = g_key_file_new();
	if (!g_key_file_load_from_file(in, file, G_KEY_FILE_NONE, NULL)) {
		g_warning("Error loading ini file :%s\n", file);
		return -1;
	}

	g = &g_group[GROUP_GLOB];
	val = g_key_file_get_string(in, g->name, g->key[GLOB_NAME], NULL);
	set_field_data(FORM_INDEX_NAME, name);
	val = g_key_file_get_string(in, g->name, g->key[GLOB_FLASHFILES], NULL);
	set_field_data(FORM_INDEX_FLASHFILES, val);
	val = g_key_file_get_string(in, g->name, g->key[GLOB_ADB_PORT], NULL);
	set_field_data(FORM_INDEX_ADB_PORT, val);
	val = g_key_file_get_string(in, g->name, g->key[GLOB_FASTBOOT_PORT], NULL);
	set_field_data(FORM_INDEX_FASTBOOT_PORT, val);

	g = &g_group[GROUP_EMUL];
	val = g_key_file_get_string(in, g->name, g->key[EMUL_PATH], NULL);
	set_field_data(FORM_INDEX_EMUL, val);

	g = &g_group[GROUP_MEM];
	val = g_key_file_get_string(in, g->name, g->key[MEM_SIZE], NULL);
	set_field_data(FORM_INDEX_MEM, val);

	g = &g_group[GROUP_VCPU];
	val = g_key_file_get_string(in, g->name, g->key[VCPU_NUM], NULL);
	set_field_data(FORM_INDEX_VCPU, val);

	g = &g_group[GROUP_FIRM];
	val = g_key_file_get_string(in, g->name, g->key[FIRM_TYPE], NULL);
	set_field_data(FORM_INDEX_FIRM, val);
	if (strcmp(val, FIRM_OPTS_UNIFIED_STR) == 0) {
		val = g_key_file_get_string(in, g->name, g->key[FIRM_PATH], NULL);
		set_field_data(FORM_INDEX_FIRM_PATH, val);
	} else if (strcmp(val, FIRM_OPTS_SPLITED_STR) == 0) {
		val = g_key_file_get_string(in, g->name, g->key[FIRM_CODE], NULL);
		set_field_data(FORM_INDEX_FIRM_CODE, val);
		val = g_key_file_get_string(in, g->name, g->key[FIRM_VARS], NULL);
		set_field_data(FORM_INDEX_FIRM_VARS, val);
	} else {
		g_warning("cannot find firmware sub-key\n");
		return -1;
	}

	g = &g_group[GROUP_DISK];
	val = g_key_file_get_string(in, g->name, g->key[DISK_SIZE], NULL);
	set_field_data(FORM_INDEX_DISK_SIZE, val);
	val = g_key_file_get_string(in, g->name, g->key[DISK_PATH], NULL);
	set_field_data(FORM_INDEX_DISK_PATH, val);

	g = &g_group[GROUP_VGPU];
	val = g_key_file_get_string(in, g->name, g->key[VGPU_TYPE], NULL);
	set_field_data(FORM_INDEX_VGPU, val);
	if (strcmp(val, VGPU_OPTS_GVTG_STR) == 0) {
		val = g_key_file_get_string(in, g->name, g->key[VGPU_GVTG_VER], NULL);
		set_field_data(FORM_INDEX_VGPU_GVTG_VER, val);

		val = g_key_file_get_string(in, g->name, g->key[VGPU_UUID], NULL);
		set_field_data(FORM_INDEX_VGPU_GVTG_UUID, val);
	}

	g = &g_group[GROUP_VTPM];
	val = g_key_file_get_string(in, g->name, g->key[VTPM_BIN_PATH], NULL);
	set_field_data(FORM_INDEX_VTPM_BIN_PATH, val);
	val = g_key_file_get_string(in, g->name, g->key[VTPM_DATA_DIR], NULL);
	set_field_data(FORM_INDEX_VTPM_DATA_DIR, val);

	g = &g_group[GROUP_RPMB];
	val = g_key_file_get_string(in, g->name, g->key[RPMB_BIN_PATH], NULL);
	set_field_data(FORM_INDEX_RPMB_BIN_PATH, val);
	val = g_key_file_get_string(in, g->name, g->key[RPMB_DATA_DIR], NULL);
	set_field_data(FORM_INDEX_RPMB_DATA_DIR, val);

	g = &g_group[GROUP_AAF];
	val = g_key_file_get_string(in, g->name, g->key[AAF_PATH], NULL);
	set_field_data(FORM_INDEX_AAF_PATH, val);

	g = &g_group[GROUP_AAF];
	val = g_key_file_get_string(in, g->name, g->key[AAF_SUSPEND], NULL);
	set_field_data(FORM_INDEX_AAF_SUSPEND, val);

	g = &g_group[GROUP_EXTRA];
	val = g_key_file_get_string(in, g->name, g->key[EXTRA_CMD], NULL);
	set_field_data(FORM_INDEX_EXTRA_CMD, val);

	g = &g_group[GROUP_PCI_PT];
	val = g_key_file_get_string(in, g->name, g->key[PCI_PT], NULL);
	set_field_data(FORM_INDEX_PCI_PT, val);

	return 0;
}

static int check_field(const char *name, char *val)
{
	char tmp[2048] = {0};

	if (strlen(val) == 0) {
		snprintf(tmp, 2048, "Error: Invalid Field [%s]", name);
		show_msg(tmp);
		return -1;
	}

	return 0;
}

int check_uuid(char *in)
{
	uuid_t u;
	return uuid_parse(in, u);
}

int generate_keyfile(void)
{
	int ret = -1;
	char file_path[MAX_PATH] = { 0 };
	char temp[1024] = { 0 };

	GKeyFile *out = g_key_file_new();

	get_field_data(FORM_INDEX_NAME, temp, sizeof(temp) - 1);
	if (0 != check_field(g_group[GROUP_GLOB].key[GLOB_NAME], temp)) {
		goto exit;
	}
	g_key_file_set_string(out, g_group[GROUP_GLOB].name, g_group[GROUP_GLOB].key[GLOB_NAME], temp);
	snprintf(file_path, sizeof(file_path) - 1, "%s/%s.ini", civ_config_path, temp);

	get_field_data(FORM_INDEX_FLASHFILES, temp, sizeof(temp) - 1);
	g_key_file_set_string(out, g_group[GROUP_GLOB].name, g_group[GROUP_GLOB].key[GLOB_FLASHFILES], temp);

	get_field_data(FORM_INDEX_ADB_PORT, temp, sizeof(temp) - 1);
	if (0 != check_field("adb port", temp)) {
		goto exit;
	}
	g_key_file_set_string(out, g_group[GROUP_GLOB].name, g_group[GROUP_GLOB].key[GLOB_ADB_PORT], temp);

	get_field_data(FORM_INDEX_FASTBOOT_PORT, temp, sizeof(temp) - 1);
	if (0 != check_field("fastboot port", temp)) {
		goto exit;
	}
	g_key_file_set_string(out, g_group[GROUP_GLOB].name, g_group[GROUP_GLOB].key[GLOB_FASTBOOT_PORT], temp);

	get_field_data(FORM_INDEX_EMUL, temp, sizeof(temp) - 1);
	if (0 != check_field(g_group[GROUP_EMUL].key[EMUL_PATH], temp)) {
		goto exit;
	}
	g_key_file_set_string(out, g_group[GROUP_EMUL].name, g_group[GROUP_EMUL].key[EMUL_PATH], temp);

	get_field_data(FORM_INDEX_MEM, temp, sizeof(temp) - 1);
	if (0 != check_field(g_group[GROUP_MEM].key[MEM_SIZE], temp)) {
		goto exit;
	}
	g_key_file_set_string(out, g_group[GROUP_MEM].name, g_group[GROUP_MEM].key[MEM_SIZE], temp);

	get_field_data(FORM_INDEX_VCPU, temp, sizeof(temp) - 1);
	if (0 != check_field(g_group[GROUP_VCPU].key[VCPU_NUM], temp)) {
		goto exit;
	}
	g_key_file_set_string(out, g_group[GROUP_VCPU].name, g_group[GROUP_VCPU].key[VCPU_NUM], temp);

	get_field_data(FORM_INDEX_FIRM, temp, sizeof(temp) - 1);
	if (0 != check_field("firmware", temp)) {
		goto exit;
	}
	g_key_file_set_string(out, g_group[GROUP_FIRM].name, g_group[GROUP_FIRM].key[FIRM_TYPE], temp);
	if (strcmp(temp, FIRM_OPTS_UNIFIED_STR) == 0) {
		get_field_data(FORM_INDEX_FIRM_PATH, temp, sizeof(temp) - 1);
		if (0 != check_field("firmware->path", temp)) {
			goto exit;
		}
		g_key_file_set_string(out, g_group[GROUP_FIRM].name, g_group[GROUP_FIRM].key[FIRM_PATH], temp);
	} else if (strcmp(temp, FIRM_OPTS_SPLITED_STR) == 0) {
		get_field_data(FORM_INDEX_FIRM_VARS, temp, sizeof(temp) - 1);
		if (0 != check_field("firmware->vars", temp)) {
			goto exit;
		}
		g_key_file_set_string(out, g_group[GROUP_FIRM].name, g_group[GROUP_FIRM].key[FIRM_VARS], temp);

		get_field_data(FORM_INDEX_FIRM_CODE, temp, sizeof(temp) - 1);
		if (0 != check_field("firmware->code", temp)) {
			goto exit;
		}
		g_key_file_set_string(out, g_group[GROUP_FIRM].name, g_group[GROUP_FIRM].key[FIRM_CODE], temp);
	} else {
		show_msg("Error: Invalid Field [firmware]");
		goto exit;
	}

	get_field_data(FORM_INDEX_DISK_SIZE, temp, sizeof(temp) - 1);
	if (0 != check_field("disk", temp)) {
		goto exit;
	}
	g_key_file_set_string(out, g_group[GROUP_DISK].name, g_group[GROUP_DISK].key[DISK_SIZE], temp);

	get_field_data(FORM_INDEX_DISK_PATH, temp, sizeof(temp) - 1);
	if (0 != check_field("disk", temp)) {
		goto exit;
	}
	g_key_file_set_string(out, g_group[GROUP_DISK].name, g_group[GROUP_DISK].key[DISK_PATH], temp);

	get_field_data(FORM_INDEX_VGPU, temp, sizeof(temp) - 1);
	if (0 != check_field("VGPU", temp)) {
		goto exit;
	}
	g_key_file_set_string(out, g_group[GROUP_VGPU].name, g_group[GROUP_VGPU].key[VGPU_TYPE], temp);
	if (strcmp(temp, VGPU_OPTS_GVTG_STR) == 0) {
		get_field_data(FORM_INDEX_VGPU_GVTG_VER, temp, sizeof(temp) - 1);
		if (0 != check_field("VGPU Version", temp)) {
			goto exit;
		}
		g_key_file_set_string(out, g_group[GROUP_VGPU].name, g_group[GROUP_VGPU].key[VGPU_GVTG_VER], temp);

		get_field_data(FORM_INDEX_VGPU_GVTG_UUID, temp, sizeof(temp) - 1);
		if (0 != check_field("vgpu uuid", temp)) {
			goto exit;
		}

		if (0 != check_uuid(temp)) {
			show_msg("Error: Invalid Field [vgpu_uuid]");
			goto exit;
		}
		g_key_file_set_string(out, g_group[GROUP_VGPU].name, g_group[GROUP_VGPU].key[VGPU_UUID], temp);
	}

	get_field_data(FORM_INDEX_VTPM_BIN_PATH, temp, sizeof(temp) - 1);
	if (0 != check_field(g_group[GROUP_VTPM].key[VTPM_BIN_PATH], temp)) {
		goto exit;
	}
	g_key_file_set_string(out, g_group[GROUP_VTPM].name, g_group[GROUP_VTPM].key[VTPM_BIN_PATH], temp);

	get_field_data(FORM_INDEX_VTPM_DATA_DIR, temp, sizeof(temp) - 1);
	if (0 != check_field(g_group[GROUP_VTPM].key[VTPM_DATA_DIR], temp)) {
		goto exit;
	}
	g_key_file_set_string(out, g_group[GROUP_VTPM].name, g_group[GROUP_VTPM].key[VTPM_DATA_DIR], temp);

	get_field_data(FORM_INDEX_RPMB_BIN_PATH, temp, sizeof(temp) - 1);
	if (0 != check_field(g_group[GROUP_RPMB].key[RPMB_BIN_PATH], temp)) {
		goto exit;
	}
	g_key_file_set_string(out, g_group[GROUP_RPMB].name, g_group[GROUP_RPMB].key[RPMB_BIN_PATH], temp);

	get_field_data(FORM_INDEX_RPMB_DATA_DIR, temp, sizeof(temp) - 1);
	if (0 != check_field(g_group[GROUP_RPMB].key[RPMB_DATA_DIR], temp)) {
		goto exit;
	}
	g_key_file_set_string(out, g_group[GROUP_RPMB].name, g_group[GROUP_RPMB].key[RPMB_DATA_DIR], temp);

	get_field_data(FORM_INDEX_AAF_PATH, temp, sizeof(temp) - 1);
	if (0 == check_field(g_group[GROUP_AAF].key[AAF_PATH], temp)) {
		g_key_file_set_string(out, g_group[GROUP_AAF].name, g_group[GROUP_AAF].key[AAF_PATH], temp);
	}

	get_field_data(FORM_INDEX_AAF_SUSPEND, temp, sizeof(temp) - 1);
	if (0 == check_field(g_group[GROUP_AAF].key[AAF_SUSPEND], temp)) {
		g_key_file_set_string(out, g_group[GROUP_AAF].name, g_group[GROUP_AAF].key[AAF_SUSPEND], temp);
	}


	get_field_data(FORM_INDEX_EXTRA_CMD, temp, sizeof(temp) - 1);
	g_key_file_set_string(out, g_group[GROUP_EXTRA].name, g_group[GROUP_EXTRA].key[EXTRA_CMD], temp);
	
	get_field_data(FORM_INDEX_PCI_PT, temp, sizeof(temp) - 1);

	g_key_file_set_string(out, g_group[GROUP_PCI_PT].name, g_group[GROUP_PCI_PT].key[PCI_PT], temp);

	g_key_file_save_to_file(out, file_path, NULL);
	ret = 0;

exit:
	g_key_file_free(out);
	return ret;
}

static int import_field(int gid, int kid, GKeyFile *in, GKeyFile *out)
{
	gchar *val;
	keyfile_group_t *group = &g_group[gid];

	val = g_key_file_get_string(in, group->name, group->key[kid], NULL);
	if (val == NULL) {
		g_warning("cannot find key %s from group [%s]!\n", group->key[kid], group->name);
		return -1;
	}
	g_key_file_set_string(out, group->name, group->key[kid], val);

	return 0;
}

static void import_extra_fields(GKeyFile *in, GKeyFile *out)
{
	size_t i;
	gsize len = 0;
	gchar **extra_keys = NULL;
	gchar *val = NULL;

	extra_keys = g_key_file_get_keys(in, "extra", &len, NULL);
	for (i = 0; i < len; i++) {
		val = g_key_file_get_string(in, "extra", extra_keys[i], NULL);
		g_key_file_set_string(out, "extra", extra_keys[i], val);
	}
}

int import_guest(char *in_path)
{
	char out_path[MAX_PATH] = {0};
	GKeyFile *in = NULL;
	GKeyFile *out = NULL;
	gchar *val = NULL;
	size_t i, j;
	keyfile_group_t *group;

	in = g_key_file_new();
	if (!g_key_file_load_from_file(in, in_path, G_KEY_FILE_NONE, NULL)) {
		g_warning("Error loading ini file :%s", in_path);
		return -1;
	}

	out = g_key_file_new();

	for (i = 0; i < GROUP_NUM; i++) {
		group = &g_group[i];
		switch(i) {
		case GROUP_GLOB:
			import_field(i, GLOB_NAME, in, out);
			import_field(i, GLOB_FLASHFILES, in, out);
			import_field(i, GLOB_ADB_PORT, in, out);
			import_field(i, GLOB_FASTBOOT_PORT, in, out);

			val = g_key_file_get_string(in, group->name, group->key[GLOB_NAME], NULL);
			if (val == NULL) {
				g_warning("cannot find key name from group [global]!\n");
				return -1;
			}
			g_key_file_set_string(out, group->name, group->key[GLOB_NAME], val);
			snprintf(out_path, 1024, "%s/%s.ini", civ_config_path, val);

			break;
		case GROUP_FIRM:
			import_field(i, FIRM_TYPE, in, out);

			val = g_key_file_get_string(in, group->name, group->key[FIRM_TYPE], NULL);
			if (strcmp(val, FIRM_OPTS_UNIFIED_STR) == 0) {
				import_field(i, FIRM_PATH, in, out);
			} else if (strcmp(val, FIRM_OPTS_SPLITED_STR) == 0) {
				import_field(i, FIRM_CODE, in, out);
				import_field(i, FIRM_VARS, in, out);
			} else {
				g_warning("cannot find firmware sub-key\n");
				return -1;
			}
			break;
		case GROUP_VGPU:
			import_field(i, VGPU_TYPE, in, out);

			val = g_key_file_get_string(in, group->name, group->key[VGPU_TYPE], NULL);
			if (strcmp(val, VGPU_OPTS_GVTG_STR) == 0) {
				import_field(i, VGPU_UUID, in, out);
				import_field(i, VGPU_GVTG_VER, in, out);
			} else if (strcmp(val, VGPU_OPTS_GVTD_STR) == 0) {
			} else if (strcmp(val, VGPU_OPTS_VIRTIO_STR) == 0) {
			} else if (strcmp(val, VGPU_OPTS_RAMFB_STR) == 0) {
			} else {
				g_warning("cannot find graphics sub-key\n");
				return -1;
			}		
			break;
		default:
			j = 0;
			while (g_group[i].key[j]) {
				import_field(i, j, in, out);
				j++;
			}
			break;
		}
	}

	import_extra_fields(in, out);

	g_key_file_save_to_file(out, out_path, NULL);

	g_key_file_free(out);

	g_key_file_free(in);

	return 0;
}

void list_guests(void)
{
	struct dirent **vm_list;
	int n, i;
	char path[MAX_PATH] = { 0 };

	printf("list VM:\n");
	printf("name\t\t\t\tstatus\n");
	printf("=======================================\n");
	n = scandir(civ_config_path, &vm_list, NULL, alphasort);
	if (n == -1) {
		fprintf(stderr, "failed to scandir:%s\n", civ_config_path);
	}
	for (i = 0; i < n; i++) {
		if (fnmatch("*.ini", vm_list[i]->d_name, FNM_PATHNAME) == 0) {
			char *guest_name = strndup(vm_list[i]->d_name, strlen(vm_list[i]->d_name) - 4);
			int active = 0;
			const char *st = "inactive";
			snprintf(path, sizeof(path) - 1, "%s/.%s"CIV_GUEST_QMP_SUFFIX, civ_config_path, guest_name);
			active = access(path, F_OK);
			if (active == 0) {
				char *result = NULL;
				if (send_qmp_cmd(path, "{\"execute\": \"query-status\"}", &result) == 0) {
					json_object *jobj = json_tokener_parse(result);
					json_object *jret, *jst;
					json_object_object_get_ex(jobj, "return", &jret);
					json_object_object_get_ex(jret, "status", &jst);
					st = json_object_get_string(jst);
					if (result)
						free(result);
				} else {
					st = "unknown";
				}
			}
			printf("%s\t\t\t\t%s\n", guest_name, st);
		}
		free(vm_list[i]);
	}
	free(vm_list);
}

int stop_guest(char *name)
{
	char path[MAX_PATH] = { 0 };
	snprintf(path, sizeof(path) - 1, "%s/.%s"CIV_GUEST_QMP_SUFFIX, civ_config_path, name);
	return send_qmp_cmd(path, "{ \"execute\": \"quit\" }", NULL);
}

int create_guest(char *name)
{
	return create_guest_tui(name);
}

int delete_guest(char *name)
{
	char cfg_file[MAX_PATH] = {0};

	snprintf(cfg_file, sizeof(cfg_file), "%s/%s.ini", civ_config_path, name);

	return remove(cfg_file);
}
