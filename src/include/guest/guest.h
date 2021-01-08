/*
 * Copyright (c) 2020 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __GUEST_H__
#define __GUEST_H__

#define PCI_DEVICE_PATH "/sys/bus/pci/devices/"
#define PCI_DRIVER_PATH "/sys/bus/pci/drivers/"

#define INTEL_GPU_BDF "0000:00:02.0"
#define INTEL_GPU_DEV_PATH PCI_DEVICE_PATH""INTEL_GPU_BDF

#define GVTG_MDEV_TYPE_PATH INTEL_GPU_DEV_PATH"/mdev_supported_types/"

#define GVTG_MDEV_V5_1_PATH INTEL_GPU_DEV_PATH"/mdev_supported_types/i915-GVTg_V5_1/"
#define GVTG_MDEV_V5_2_PATH INTEL_GPU_DEV_PATH"/mdev_supported_types/i915-GVTg_V5_2/"
#define GVTG_MDEV_V5_4_PATH INTEL_GPU_DEV_PATH"/mdev_supported_types/i915-GVTg_V5_4/"
#define GVTG_MDEV_V5_8_PATH INTEL_GPU_DEV_PATH"/mdev_supported_types/i915-GVTg_V5_8/"

#ifndef UUID_STR_LEN
#define UUID_STR_LEN 37U
#endif

enum {
	MENU_INDEX_SAVE = 0,
	MENU_INDEX_EXIT,
	MENU_NUM
};

typedef enum {
	FORM_INDEX_NAME = 0,
	FORM_INDEX_FLASHFILES,
	FORM_INDEX_EMUL,
	FORM_INDEX_MEM,
	FORM_INDEX_VCPU,
	FORM_INDEX_FIRM,
	FORM_INDEX_FIRM_PATH, FORM_INDEX_FIRM_CODE = 6,
	FORM_INDEX_FIRM_VARS,
	FORM_INDEX_DISK_SIZE,
	FORM_INDEX_DISK_PATH,
	FORM_INDEX_VGPU,
	FORM_INDEX_VGPU_GVTG_VER,
	FORM_INDEX_VGPU_GVTG_UUID,
	FORM_INDEX_ADB_PORT,
	FORM_INDEX_FASTBOOT_PORT,
	FORM_INDEX_VTPM_BIN_PATH,
	FORM_INDEX_VTPM_DATA_DIR,
	FORM_INDEX_RPMB_BIN_PATH,
	FORM_INDEX_RPMB_DATA_DIR,
	FORM_INDEX_PCI_PT,
	FORM_INDEX_AAF_PATH,
	FORM_INDEX_EXTRA_CMD,
	FORM_NUM
} form_index_t;

enum {
	FIRM_OPTS_UNIFIED = 0,
	FIRM_OPTS_SPLITED
};

#define FIRM_OPTS_UNIFIED_STR "unified"
#define FIRM_OPTS_SPLITED_STR "splited"

enum {
	VGPU_OPTS_VIRTIO = 0,
	VGPU_OPTS_SW,
	VGPU_OPTS_GVTG,
	VGPU_OPTS_GVTD
};

#define VGPU_OPTS_VIRTIO_STR   "virtio"
#define VGPU_OPTS_SW_STR       "swrender"
#define VGPU_OPTS_GVTG_STR     "GVT-g"
#define VGPU_OPTS_GVTD_STR     "GVT-d"

enum {
	GVTG_OPTS_V5_1 = 0,
	GVTG_OPTS_V5_2,
	GVTG_OPTS_V5_4,
	GVTG_OPTS_V5_8,
};

#define GVTG_OPTS_V5_1_STR "i915-GVTg_V5_1"
#define GVTG_OPTS_V5_2_STR "i915-GVTg_V5_2"
#define GVTG_OPTS_V5_4_STR "i915-GVTg_V5_4"
#define GVTG_OPTS_V5_8_STR "i915-GVTg_V5_8"

#define PT_MAX 64		// Maximum number of passthrough devices

enum {
	FIELD_TYPE_NORMAL = 0,
	FIELD_TYPE_ALNUM,
	FIELD_TYPE_INTEGER,
	FIELD_TYPE_STATIC
};

typedef struct {
	const char *name;
	const char *key[64];
} keyfile_group_t;

enum {
	GROUP_GLOB = 0,
	GROUP_EMUL,
	GROUP_MEM,
	GROUP_VCPU,
	GROUP_FIRM,
	GROUP_DISK,
	GROUP_VGPU,
	GROUP_VTPM,
	GROUP_RPMB,
	GROUP_AAF,
	GROUP_EXTRA,
	GROUP_PCI_PT,
	GROUP_NUM
};

enum {
	/* Sub key of group glob */
	GLOB_NAME = 0,
	GLOB_FLASHFILES,
	GLOB_ADB_PORT,
	GLOB_FASTBOOT_PORT,

	/* Sub key of group emul */
	EMUL_PATH = 0,

	/* Sub key of group mem */
	MEM_SIZE = 0,

	/* Sub key of group num */
	VCPU_NUM = 0,

	/* Sub key of group firm */
	FIRM_TYPE = 0,
	FIRM_PATH,
	FIRM_CODE,
	FIRM_VARS,

	/* Sub key of group disk */
	DISK_PATH = 0,
	DISK_SIZE,

	/* Sub key of group vgpu */
	VGPU_TYPE = 0,
	VGPU_GVTG_VER,
	VGPU_UUID,

	/* Sub key of group vtpm */
	VTPM_BIN_PATH = 0,
	VTPM_DATA_DIR,

	/* Sub key of group rpmb */
	RPMB_BIN_PATH = 0,
	RPMB_DATA_DIR,

	/* Sub key of group aaf */
	AAF_PATH = 0,

	/* Sub key of group vgpu */
	EXTRA_CMD = 0,

	/* Sub key of group pci passthrough */
	PCI_PT = 0,
};

void show_msg(const char *msg);
int set_field_data(form_index_t i, const char *str);
int get_field_data(form_index_t i, char *str, size_t len);
int generate_keyfile(void);
int load_form_data(char *name);
int check_uuid(char *in);

int create_guest_tui(char *name);

int send_qmp_cmd(const char *path, const char *cmd, char **r);

#endif /* __GUEST_H__ */
