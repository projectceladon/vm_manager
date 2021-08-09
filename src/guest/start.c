/*
 * Copyright (c) 2020 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <uuid/uuid.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <regex.h>
#include <string.h>
#include "vm_manager.h"
#include "guest.h"
#include "utils.h"
#include "rpmb.h"
#include "vtpm.h"
#include "aaf.h"


extern keyfile_group_t g_group[];

static const char *fixed_cmd =
	" -machine type=q35,kernel_irqchip=off"
	" -k en-us"
	" -cpu host"
	" -enable-kvm"
	" -device qemu-xhci,id=xhci,p2=8,p3=8"
	" -device usb-mouse"
	" -device usb-kbd"
	" -device intel-hda -device hda-duplex"
	" -audiodev id=android_spk,timer-period=5000,driver=pa"
	" -device e1000,netdev=net0"
	" -device intel-iommu,device-iotlb=on,caching-mode=on"
	" -nodefaults ";

/* Used to keep track of pcis that are passed through */
#define PT_LEN 16
static char *pci_pt_record[PT_MAX] = { 0 };
static int pci_count = 0;

static int create_vgpu(GKeyFile *gkf)
{
	int fd = 0;
	ssize_t n = 0;
	g_autofree gchar *gvtg_ver = NULL, *uuid = NULL;
	keyfile_group_t *g = &g_group[GROUP_VGPU];
	char file[MAX_PATH] = { 0 };

	gvtg_ver = g_key_file_get_string(gkf, g->name, g->key[VGPU_GVTG_VER], NULL);
	if (gvtg_ver == NULL) {
		g_warning("Invalid GVTg version\n");
		return -1;
	}
	snprintf(file, MAX_PATH, GVTG_MDEV_TYPE_PATH"%s/create", gvtg_ver);

	uuid = g_key_file_get_string(gkf, g->name, g->key[VGPU_UUID], NULL);
	if (uuid == NULL) {
		g_warning("Invalid VGPU UUID\n");
		return -1;
	}

	fd = open(file, O_WRONLY);
	if (fd == -1) {
		fprintf(stderr, "open %s failed, errno=%d\n", file, errno);
		return -1;
	}

	n = strlen(uuid);
	if (write(fd, uuid, n) != n) {
		fprintf(stderr, "write %s failed, errno=%d\n", file, errno);
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}

static int passthrough_gpu(void)
{
	int fd = 0;
	ssize_t n = 0;
	int dev_id = 0;
	char buf[64] = { 0 };
	struct stat st;
	int pid;
	int wst;

	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "%s: Failed to fork.", __func__);
		return -1;
	} else if (pid == 0) {
		execlp("modprobe", "modprobe", "vfio", NULL);
		return -1;
	} else {
		wait(&wst);
		if (!(WIFEXITED(wst) && !WEXITSTATUS(wst))) {
			fprintf(stderr, "Failed to load module: vfio\n");
			return -1;
		}
	}

	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "%s: Failed to fork.", __func__);
		return -1;
	} else if (pid == 0) {
		execlp("modprobe", "modprobe", "vfio-pci", NULL);
		return -1;
	} else {
		wait(&wst);
		if (!(WIFEXITED(wst) && !WEXITSTATUS(wst))) {
			fprintf(stderr, "Failed to load module: vfio-pci\n");
			return -1;
		}
	}

	/* Get device ID */
	fd = open(INTEL_GPU_DEV_PATH"/device", O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "open %s failed, errno=%d\n", INTEL_GPU_DEV_PATH"/device", errno);
		return -1;
	}

	n = read(fd, buf, sizeof(buf));
	if (n == -1) {
		fprintf(stderr, "read %s failed, errno=%d\n", INTEL_GPU_DEV_PATH"/device", errno);
		close(fd);
		return -1;
	}
	dev_id = strtol(buf, NULL, 16);

	close(fd);

	if (stat(INTEL_GPU_DEV_PATH"/driver", &st) == 0) {
		/* Unbind original driver */
		fd = open(INTEL_GPU_DEV_PATH"/driver/unbind", O_WRONLY);
		if (fd == -1) {
			fprintf(stderr, "open %s failed, errno=%d\n", INTEL_GPU_DEV_PATH"/driver/unbind", errno);
			return -1;
		}

		n = strlen(INTEL_GPU_BDF);
		if (write(fd, INTEL_GPU_BDF, n) != n) {
			fprintf(stderr, "write %s failed, errno=%d\n", INTEL_GPU_DEV_PATH"/driver/unbind", errno);
			close(fd);
			return -1;
		}

		close(fd);
	}

	/* Create new vfio id for GPU */
	fd = open(PCI_DRIVER_PATH"vfio-pci/new_id", O_WRONLY);
	if (fd == -1) {
		fprintf(stderr, "open %s failed, errno=%d\n", PCI_DRIVER_PATH"vfio-pci/new_id", errno);
		return -1;
	}

	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "8086 %x", dev_id);
	n = strlen(buf);
	if (write(fd, buf, n) != n) {
		fprintf(stderr, "write %s failed, errno=%d\n", PCI_DRIVER_PATH"vfio-pci/new_id", errno);
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}

void system_call(const char *process, const char *args) {
	pid_t pid;
	int stat;

	if (0 == (pid = fork())) {
		execl(process, args, (char *)NULL);
		char error_msg[128];
		sprintf(error_msg, "%s command failed.", process);
		perror(error_msg);
		exit(1);
	} else {
		wait(&stat);
	}
	if (WIFEXITED(stat))
		printf("Exit status: %d\n", WEXITSTATUS(stat));
	else if (WIFSIGNALED(stat))
		psignal(WTERMSIG(stat), "Exit signal");
}

static int setup_passthrough(char *pci_device, int unset);

static void cleanup_passthrough(void) {
	for (int i=0; i<1; i++) {
		fprintf(stderr, "pci unset: %s\n", pci_pt_record[i]);
		setup_passthrough(pci_pt_record[i], 1);
		free(pci_pt_record[i]);
	}
}

static void cleanup(int num)
{
	(void)num;
	fprintf(stderr, "Cleaning up...\n");
	cleanup_child_proc();
	cleanup_rpmb();
	cleanup_passthrough();

	exit(130);
}

void set_cleanup(void) 
{
	signal(SIGINT, cleanup); 
}

static int check_driver_unbinded(const char *driver) {
	/* TODO: check driver unbinded */
	(void)driver;
	sleep(1);
	return 0;
}

static int setup_passthrough(char *pci_device, int unset) {

	char iommu_grp_dev[64] = {0};
	DIR *dp;
	struct dirent *ep;

	snprintf(iommu_grp_dev, 64, "/sys/bus/pci/devices/%s/iommu_group/devices", pci_device);
	dp = opendir(iommu_grp_dev);

	if (dp == NULL) {
		fprintf(stderr, "Couldn't open the directory");
		return -1;
	}

	/* Iterate through devices in iommu group directory */
	while ((ep = readdir (dp))) {
		char *pci_device = ep->d_name;
		if (strcmp(pci_device, ".") == 0 || strcmp(pci_device, "..") == 0) {
			continue;
		}
		char buffer[512] = {0}, device[64] = {0}, vendor[64] = {0};
		char *rp = NULL;
			
		/* Get device_id and vendor_id */
		snprintf(buffer, 512, "%s/%s/device", iommu_grp_dev, pci_device);
		fprintf(stderr, "%s\n", buffer);
		rp = canonicalize_file_name(buffer);
		if (!rp)
			continue;
		FILE *f = fopen(rp, "r");
		if (f == NULL)
			continue;
		if (!fgets(device, sizeof(device), f)) {
			fprintf(stderr, "Cannot get device name for device %s.", pci_device);
			fclose(f);
			continue;
		}
		fclose(f);
		free(rp); rp = NULL;

		memset(buffer, 0, sizeof(buffer));
		snprintf(buffer, 512, "%s/%s/vendor", iommu_grp_dev, pci_device);
		rp = canonicalize_file_name(buffer);
		if (!rp)
			continue;
		f = fopen(rp, "r");
		if (f == NULL)
			continue;
		if (!fgets(vendor, sizeof(vendor), f)) {
			fprintf(stderr, "Cannot get vendor name for device %s.", pci_device);
			fclose(f);
			continue;
		}
		fclose(f);
		free(rp); rp = NULL;

		if (unset) {
			memset(buffer, 0, sizeof(buffer));
			snprintf(buffer, 512, "%s/%s/driver", iommu_grp_dev, pci_device);

			char *driver_in_use = basename(canonicalize_file_name(buffer));
			fprintf(stderr, "driverinuse %s %d\n", driver_in_use, strcmp(driver_in_use, "vfio-pci"));
			if (strcmp(driver_in_use, "vfio-pci") == 0) {
				fprintf(stderr, "unbind %s\n", pci_device);
				write_to_file("/sys/bus/pci/drivers/vfio-pci/unbind", pci_device);
				snprintf(buffer, sizeof(buffer), "%.6s %.6s", vendor, device);
				// "echo %s %s > /sys/bus/pci/drivers/vfio-pci/remove_id"
				fprintf(stderr, "remove_id %s\n", buffer);
				write_to_file("/sys/bus/pci/drivers/vfio-pci/remove_id", buffer);
			}
			// free(driver_in_use);
			if (check_driver_unbinded(pci_device) == 0) {
				fprintf(stderr, "drivers_probe reset %s\n", pci_device);
				write_to_file("/sys/bus/pci/drivers_probe", pci_device);
			} else {
				fprintf(stderr, "Failed to unbind driver or did not finish in the given time frame.\n");
			}
		} else {
			/* Check if driver exists, unbind if it exists */
			memset(buffer, 0, sizeof(buffer));
			snprintf(buffer, 512, "%s/%s/driver", iommu_grp_dev, pci_device);
			rp = canonicalize_file_name(buffer);
			if (!rp)
				continue;
			DIR* dir = opendir(rp);
			if (dir) {
				/* Driver directory exists. */
				snprintf(buffer, 512, "%s/%s/driver/unbind", iommu_grp_dev, pci_device);
				write_to_file(buffer, pci_device);

				closedir(dir);
			} else if (ENOENT == errno) {
				/* Directory does not exist. */
				// fprintf(stderr, "%s pci device could not unbind from current driver. Driver directory does not exist. ", pci_device);
				// continue;
			} else {
				/* opendir() failed for some other reason. */
				// fprintf(stderr, "%s pci device could not unbind from current driver. Driver directory does not exist. ", pci_device);
				// continue;
			}
			free(rp); rp = NULL;

			memset(buffer, 0, sizeof(buffer));
			snprintf(buffer, sizeof(buffer), "%s %s", vendor, device);

			write_to_file(PCI_DRIVER_PATH"vfio-pci/new_id", buffer);
		}
	}
	(void) closedir (dp);

	return 0;
}

static int setup_passthrough_pci(char *pci_device, char *p, size_t size) {

	int cx = 0;

	if (setup_passthrough(pci_device, 0) == 0) {
		cx = snprintf(p, size, " -device vfio-pci,host=%s,x-no-kvm-intx=on", pci_device);
	} 

	return cx;
} 

static int run_battery_mediation_daemon(char *path) {
	return execute_cmd(path, NULL, 0, 1);
}

static int run_thermal_mediation_daemon(char *path) {
	return execute_cmd(path, NULL, 0, 1);
}

static int run_guest_timekeep(char *path, char *p, size_t size, char *pipe_name) {
	int cx = 0; 
	const char *pipe = pipe_name ? pipe_name : "qmp-time-keep-pipe";
	char buffer[1024] = {0}, dirn[1024] = {0};
	strncpy(dirn, path, 1023);
	snprintf(buffer, 1023, "%s/%s", dirname(dirn), pipe);
	fprintf(stderr, "Timekeep: %s %s\n", path, buffer);
	int ret = execute_cmd(path, buffer, strlen(buffer), 1);
	if (ret == 0) {
		cx = snprintf(p, size, " -qmp pipe:%s", buffer);
	}
	return cx;
}

static int run_guest_pm(char *path, char *p, size_t size, char *socket_name) {
	int cx = 0; 
	const char *socket = socket_name ? socket_name : "qmp-pm-sock";
	char buffer[1024] = {0}, dirn[1024] = {0};
	snprintf(buffer, 1023, "%s/%s", dirname(dirn), socket);
	fprintf(stderr, "PM: %s %s\n", path, buffer);
	int ret = execute_cmd(path, buffer, strlen(buffer), 1);
	if (ret == 0) {
		cx = snprintf(p, size, " -qmp unix:%s,server,nowait -no-reboot", buffer);
	}
	return cx;
}

static int run_extra_service(char *str)
{
	gchar **cmd = NULL;
	struct stat st;
	int ret = 0;
	int arg_len = 0;

	cmd = g_strsplit(str, " ", 2);
	if (!cmd)
		return -1;

	if (stat(cmd[0], &st) != 0)
		return -1;

	if (cmd[1])
		arg_len = strlen(cmd[1]);

	ret = execute_cmd(cmd[0], cmd[1], arg_len, 1);

	g_strfreev(cmd);

	return ret;
}

int start_guest(char *name)
{
	int ret = 0;
	int cx;
	GKeyFile *gkf;
	g_autofree gchar *val = NULL;
	char cfg_file[MAX_PATH] = { 0 };
	char emu_path[MAX_PATH] = { 0 };
	char cmd_str[MAX_CMDLINE_LEN] = { 0 };
	char *p = &cmd_str[0];
	size_t size = sizeof(cmd_str);
	keyfile_group_t *g = NULL;

	if (!name) {
		fprintf(stderr, "%s: Invalid input param\n", __func__);
		return -1;
	}

	snprintf(cfg_file, sizeof(cfg_file), "%s/%s.ini", civ_config_path, name);

	gkf = g_key_file_new();

	if (!g_key_file_load_from_file(gkf, cfg_file, G_KEY_FILE_NONE, NULL)) {
		g_warning("Error loading ini file :%s", cfg_file);
		return -1;
	}

	g = &g_group[GROUP_EMUL];
	val = g_key_file_get_string(gkf, g->name, g->key[EMUL_PATH], NULL);
	if (val == NULL) {
		g_warning("cannot find key emulator path from group general\n");
		return -1;
	}
	snprintf(emu_path, sizeof(emu_path), "%s", val);
	fprintf(stderr, "emu_path: %s\n", emu_path);

	g = &g_group[GROUP_GLOB];
	val = g_key_file_get_string(gkf, g->name, g->key[GLOB_NAME], NULL);
	if (val == NULL) {
		g_warning("cannot find key name from group global\n");
		return -1;
	}
	cx = snprintf(p, size, " -name %s", val);
	p += cx; size -= cx;

	cx = snprintf(p, size, " -qmp unix:%s/.%s"CIV_GUEST_QMP_SUFFIX",server,nowait", civ_config_path, val);
	p += cx; size -= cx;

	val = g_key_file_get_string(gkf, g->name, g->key[GLOB_ADB_PORT], NULL);
	cx = snprintf(p, size, " -netdev user,id=net0,hostfwd=tcp::%s-:5555", val);
	p += cx; size -= cx;

	val = g_key_file_get_string(gkf, g->name, g->key[GLOB_FASTBOOT_PORT], NULL);
	cx = snprintf(p, size, ",hostfwd=tcp::%s-:5554", val);
	p += cx; size -= cx;

	/*
	 * Please keep RPMB device option to be the first virtio device in QEMU command line. Since secure
	 * storage daemon in Andriod side communicates with /dev/vport0p1 for RPMB usage, this is a
	 * limitation for Google RPMB solution. If any other virtio devices are passed to QEMU before RPMB,
	 * virtual port device node will be no longer named /dev/vport0p1, it leads secure storage daemon
	 * working abnormal.
	 */
	g = &g_group[GROUP_RPMB];
	val = g_key_file_get_string(gkf, g->name, g->key[RPMB_BIN_PATH], NULL);
	if (val) {
		if (0 == set_rpmb_bin_path(val)) {
			val = g_key_file_get_string(gkf, g->name, g->key[RPMB_DATA_DIR], NULL);
			if (val) {
				if (0 == set_rpmb_data_dir(val)) {
					cx = snprintf(p, size, " -device virtio-serial,addr=1 -device virtserialport,chardev=rpmb0,name=rpmb0,nr=1 -chardev socket,id=rpmb0,path=%s/%s",
							val, RPMB_SOCK);
					p += cx; size -= cx;
				}
			}
		}
	}

	g = &g_group[GROUP_VTPM];
	val = g_key_file_get_string(gkf, g->name, g->key[VTPM_BIN_PATH], NULL);
	if (val) {
		if (0 == set_vtpm_bin_path(val)) {
			val = g_key_file_get_string(gkf, g->name, g->key[VTPM_DATA_DIR], NULL);
			if (val) {
				if (0 == set_vtpm_data_dir(val)) {
					cx = snprintf(p, size, " -chardev socket,id=chrtpm,path=%s/%s -tpmdev emulator,id=tpm0,chardev=chrtpm -device tpm-crb,tpmdev=tpm0",
							val, SWTPM_SOCK);
					p += cx; size -= cx;
				}
			}
		}
	}

	g = &g_group[GROUP_AAF];
	val = g_key_file_get_string(gkf, g->name, g->key[AAF_PATH], NULL);
	if (val) {
		if (set_aaf_path(val)) {
			fprintf(stderr, "Failed to set aaf path! val=%s\n", val);
			return -1;
		}
		cx = snprintf(p, size, " -fsdev local,security_model=none,id=fsdev_aaf,path=%s -device virtio-9p-pci,fsdev=fsdev_aaf,mount_tag=aaf,addr=3", val);
		p += cx; size -= cx;

		val = g_key_file_get_string(gkf, g->name, g->key[AAF_SUSPEND], NULL);
		if (val) {
			if (0 == strcmp(val, SUSPEND_ENABLE_STR))
				set_aaf_option(AAF_CONFIG_SUSPEND, AAF_SUSPEND_ENABLE);
			else if (0 == strcmp(val, SUSPEND_DISABLE_STR))
				set_aaf_option(AAF_CONFIG_SUSPEND, AAF_SUSPEND_DISABLE);
			else
				g_warning("Invalid setting of AAF Allow suspend option, it should be either true or false\n");
		}
	}

	g = &g_group[GROUP_VGPU];
	val = g_key_file_get_string(gkf, g->name, g->key[VGPU_TYPE], NULL);
	if (val == NULL) {
		g_warning("cannot find key name from group graphics\n");
		return -1;
	}
	if (strcmp(val, VGPU_OPTS_GVTG_STR) == 0) {
		char vgpu_path[MAX_PATH] = { 0 };
		struct stat st;
		val = g_key_file_get_string(gkf, g->name, g->key[VGPU_UUID], NULL);
		if (val == NULL) {
			g_warning("Invalid VGPU\n");
			return -1;
		}

		if (check_uuid(val) != 0) {
			fprintf(stderr, "Invalid UUID format!\n");
			return -1;
		}

		snprintf(vgpu_path, sizeof(vgpu_path), INTEL_GPU_DEV_PATH"/%s", val);
		if (stat(vgpu_path, &st) != 0) {
			if (create_vgpu(gkf) == -1) {
				g_warning("failed to create vGPU\n");
				return -1;
			}
		}
		cx = snprintf(p, size, " -display gtk,gl=on -device vfio-pci-nohotplug,ramfb=on,sysfsdev=%s,display=on,addr=2.0,x-igd-opregion=on", vgpu_path);
		p += cx; size -= cx;
		set_aaf_option(AAF_CONFIG_GPU_TYPE, AAF_GPU_TYPE_GVTG);
	} else if (strcmp(val, VGPU_OPTS_GVTD_STR) == 0) {
		if (passthrough_gpu())
			return -1;
		cx = snprintf(p, size, " -vga none -nographic -device vfio-pci,host=00:02.0,x-igd-gms=2,id=hostdev0,bus=pcie.0,addr=0x2,x-igd-opregion=on");
		p += cx; size -= cx;
		set_aaf_option(AAF_CONFIG_GPU_TYPE, AAF_GPU_TYPE_GVTD);
	} else if (strcmp(val, VGPU_OPTS_VIRTIO_STR) == 0) {
		cx = snprintf(p, size, " -display gtk,gl=on -device virtio-gpu-pci");
		p += cx; size -= cx;
		set_aaf_option(AAF_CONFIG_GPU_TYPE, AAF_GPU_TYPE_VIRTIO);
	} else if (strcmp(val, VGPU_OPTS_RAMFB_STR) == 0) {
		cx = snprintf(p, size, " -display gtk,gl=on -device ramfb");
		p += cx; size -= cx;
	} else {
		g_warning("Invalid Graphics config\n");
		return -1;
	}

	g = &g_group[GROUP_MEM];
	val = g_key_file_get_string(gkf, g->name, g->key[MEM_SIZE], NULL);
	if (val == NULL) {
		g_warning("cannot find key name from group memory\n");
		return -1;
	}
	cx = snprintf(p, size, " -m %s", val);
	p += cx; size -= cx;

	g = &g_group[GROUP_VCPU];
	val = g_key_file_get_string(gkf, g->name, g->key[VCPU_NUM], NULL);
	if (val == NULL) {
		g_warning("cannot find key name from group vcpu\n");
		return -1;
	}
	cx = snprintf(p, size, " -smp %s", val);
	p += cx; size -= cx;

	g = &g_group[GROUP_FIRM];
	val = g_key_file_get_string(gkf, g->name, g->key[FIRM_TYPE], NULL);
	if (val == NULL) {
		g_warning("cannot find key name from group firmware\n");
		return -1;
	}
	if (strcmp(val, FIRM_OPTS_UNIFIED_STR) == 0) {
		val = g_key_file_get_string(gkf, g->name, g->key[FIRM_PATH], NULL);
		cx = snprintf(p, size, " -drive if=pflash,format=raw,file=%s", val);
		p += cx; size -= cx;
	} else if (strcmp(val, FIRM_OPTS_SPLITED_STR) == 0) {
		val = g_key_file_get_string(gkf, g->name, g->key[FIRM_CODE], NULL);
		cx = snprintf(p, size, " -drive if=pflash,format=raw,readonly,file=%s", val);
		p += cx; size -= cx;
		val = g_key_file_get_string(gkf, g->name, g->key[FIRM_VARS], NULL);
		cx = snprintf(p, size, " -drive if=pflash,format=raw,file=%s", val);
		p += cx; size -= cx;
	} else {
		g_warning("cannot find firmware sub-key\n");
		return -1;
	}

	g = &g_group[GROUP_DISK];
	val = g_key_file_get_string(gkf, g->name, g->key[DISK_PATH], NULL);
	if (val == NULL) {
		g_warning("cannot find key name from group disk\n");
		return -1;
	}
	cx = snprintf(p, size, " -drive file=%s,if=none,id=disk1,discard=unmap,detect-zeroes=unmap -device virtio-blk-pci,drive=disk1,bootindex=1", val);
	p += cx; size -= cx;

	g = &g_group[GROUP_PCI_PT];

	val = g_key_file_get_string(gkf, g->name, g->key[PCI_PT], NULL);
	if (val == NULL || !strcmp("", val))
		goto SKIP_PT;

	char opts[PT_MAX][20];
	char *temp[PT_MAX];

	for (int i=0; i<PT_MAX; i++) {
		temp[i] = opts[i];
	}

	int res_count = find_pci("", PT_MAX, temp);
	pci_count = 0;

	if (res_count > 0) {
		/* Add vfio-pci kernel module, needs sudo privilege */	
		if ((ret = load_kernel_module("vfio")) != 0) {
			fprintf(stderr, "vfio error\n");
			goto SKIP_PT;
		}
		
		if ((ret = load_kernel_module("vfio-pci")) != 0) {
			fprintf(stderr, "vfio-pci error\n");
			goto SKIP_PT;
		}
	}

	ret = 0;
	char delim[] = ",";
	char *ptr = strtok(val, delim);

	while (ptr != NULL) {
		for (int i=0; i<res_count; i++) {
			if (strcmp(ptr, temp[i]) == 0) {
				cx = setup_passthrough_pci(ptr, p, size);
				p += cx; size -= cx;

				if (cx != 0) {
					pci_pt_record[pci_count] = malloc(PT_MAX);
					strncpy(pci_pt_record[pci_count], ptr, PT_MAX-1);
					pci_count++;
				}

				break;
			}
		}
		ptr = strtok(NULL, delim);
	}

SKIP_PT:
	if (ret != 0)
		fprintf(stderr, "Passthrough not enabled due to failure to load vfio modules.\n");

	/* run mediation processes */

	g = &g_group[GROUP_MEDIATION];

	val = g_key_file_get_string(gkf, g->name, g->key[BATTERY_MED], NULL);
	if (val != NULL && (strcmp("", val) != 0))
		run_battery_mediation_daemon(val);
	
	val = g_key_file_get_string(gkf, g->name, g->key[THERMAL_MED], NULL);
	if (val != NULL && (strcmp("", val) != 0))
		run_thermal_mediation_daemon(val);

	/* run guest pm */
	g = &g_group[GROUP_GUEST_SERVICE];

	val = g_key_file_get_string(gkf, g->name, g->key[GUEST_TIME_KEEP], NULL);
	if (val != NULL && (strcmp("", val) != 0)) {
		cx = run_guest_timekeep(val, p, size, NULL);
		p += cx; size -= cx;
	}

	/* run guest time keep */

	val = g_key_file_get_string(gkf, g->name, g->key[GUEST_PM], NULL);
	if (val != NULL && (strcmp("", val) != 0)) {
		cx = run_guest_pm(val, p, size, NULL);
		p += cx; size -= cx;
	}

	/* extra GROUP */
	g = &g_group[GROUP_EXTRA];
	/* extra cmds */
	val = g_key_file_get_string(gkf, g->name, g->key[EXTRA_CMD], NULL);
	if (val != NULL && (strcmp("", val) != 0)) {
		cx = snprintf(p, size, " %s", val);
		printf("%s: %s\n", g->key[EXTRA_CMD], val);
		p += cx; size -= cx;
	}
	/* extra services */
	gchar **srv_list = NULL;
	gsize n_extra_srv = 0;
	srv_list = g_key_file_get_string_list(gkf, g->name, g->key[EXTRA_SERVICE], &n_extra_srv, NULL);
	if (srv_list != NULL && (n_extra_srv != 0)) {
		gsize i;
		for (i = 0; i < n_extra_srv; i++) {
			g_strstrip(srv_list[i]);
			if (strcmp("", srv_list[i])) {
				run_extra_service(srv_list[i]);
			}
		}
		g_strfreev(srv_list);
	}

	cx = snprintf(p, size, "%s", fixed_cmd);
	p += cx; size -= cx;

	run_vtpm_daemon();

	cleanup_rpmb();
	run_rpmb_daemon();

	flush_aaf_config();

	fprintf(stderr, "run: %s %s\n", emu_path, cmd_str);

	ret = execute_cmd(emu_path, cmd_str, strlen(cmd_str), 0);
	
	cleanup(0);

	if (ret != 0) {
		err(1, "%s:Failed to execute emulator command, err=%d\n", __func__, errno);
		return -1;
	}

	g_key_file_free(gkf);
	return 0;
}
