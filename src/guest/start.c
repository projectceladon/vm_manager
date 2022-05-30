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
#include <ctype.h>
#include "vm_manager.h"
#include "guest.h"
#include "utils.h"
#include "rpmb.h"
#include "vtpm.h"
#include "aaf.h"

extern keyfile_group_t g_group[];

static const char *fixed_cmd =
	" -M q35"
	" -machine kernel_irqchip=on"
	" -k en-us"
	" -cpu host,-waitpkg"
	" -enable-kvm"
	" -device qemu-xhci,id=xhci,p2=8,p3=8"
	" -device usb-mouse"
	" -device usb-kbd"
	" -device intel-iommu,device-iotlb=on,caching-mode=on"
	" -nodefaults ";

static int get_uid() {
	char *suid = NULL;
	int real_uid;
	suid = getenv("SUDO_UID");
	printf ("suid=%s\n", suid);
	if (suid) {
		real_uid = atoi(suid);
	} else {
		real_uid = getuid();
	}
	return real_uid;
}
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

static int is_vfio_driver(const char *driver)
{
	ssize_t nbytes;
	struct stat st;
	char buf[PATH_MAX] = { 0 };
	char *driver_name;
	if (!driver)
		return -1;

	if (lstat(driver, &st) == -1) {
		printf("%s: lstat failed, errno=%d!\n", __func__, errno);
		return -1;
	}

	nbytes = readlink(driver, buf, PATH_MAX);
	if (nbytes == -1) {
		printf("%s: readlink fail! errno=%d\n", __func__, errno);
		return -1;
	}

	driver_name = basename(buf);
	if (strncmp(driver_name, "vfio-pci", 8) == 0)
		return 0;

	return -1;
}

static int remove_sof_tgl_snd_module()
{
	int pid;
	int wst;
	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "%s: Failed to fork.", __func__);
		return -1;
	} else if (pid == 0) {
		execlp("rmmod", "rmmod", "snd-sof-pci-intel-tgl", NULL);
		return -1;
	}else {
		wait(&wst);
		if (!(WIFEXITED(wst) && !WEXITSTATUS(wst))) {
			fprintf(stderr, "Failed to load module: snd-sof-pci-intel-tgl\n");
			return -1;
		}
	}
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
	int ret = 0;

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

	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "8086 %x", dev_id);

	if (stat(INTEL_GPU_DEV_PATH"/driver", &st) == 0) {
		if (is_vfio_driver(INTEL_GPU_DEV_PATH"/driver") == 0) {
			if (write_to_file("/sys/bus/pci/drivers/vfio-pci/remove_id", buf)) {
				fprintf(stderr, "Cannot remove original GPU vfio id\n");
				return -1;
			}
		}
		/* Unbind original driver */
		if (write_to_file(INTEL_GPU_DEV_PATH"/driver/unbind", INTEL_GPU_BDF)) {
			fprintf(stderr, "%s: Cannot unbind original driver!\n", __func__);
		}
	}


	/* Create new vfio id for GPU */
	ret = write_to_file(PCI_DRIVER_PATH"vfio-pci/new_id", buf);
	if (ret == EEXIST) {
		if (write_to_file("/sys/bus/pci/drivers/vfio-pci/remove_id", buf)) {
			fprintf(stderr, "Cannot remove original GPU vfio id\n");
			return -1;
		}
		if (write_to_file(PCI_DRIVER_PATH"vfio-pci/new_id", buf)) {
			fprintf(stderr, "Cannot add new GPU vfio id\n");
			return -1;
		}
	} else if (ret != 0) {
		return -1;
	}

	pci_pt_record[pci_count] = malloc(PT_MAX);
	strncpy(pci_pt_record[pci_count], INTEL_GPU_BDF, PT_MAX-1);
	pci_count++;

	return 0;
}

static int setup_hugepages(GKeyFile *gkf)
{
	int fd = 0;
	ssize_t n = 0;
	int required_hugepg = 0;
	int free_hugepg = 0;
	int nr_hugepg = 0;
	char buf[64] = { 0 };
	keyfile_group_t *g = NULL;
	g_autofree gchar *val = NULL;
	size_t len = 0;
	char ch;
	char *pEnd = NULL;
	int memsize_M;
	int wait_cnt = 0;

	g = &g_group[GROUP_MEM];
	val = g_key_file_get_string(gkf, g->name, g->key[MEM_SIZE], NULL);
	if (val == NULL) {
		g_warning("cannot find key name from group memory\n");
		return 0;
	}
	len = strlen(val);
	if (len == 0) {
		fprintf(stderr, "Memory size for guest not specified!\n");
		return 0;
	}
	if (isdigit(val[len - 1])) {
		memsize_M = strtoul(val, &pEnd, 10);
		if (pEnd != &val[len - 1]) {
			fprintf(stderr, "Invalid memory size format!\n");
			return 0;
		}
	} else {
		memsize_M = strtoul(val, &pEnd, 10);
		if (pEnd != &val[len - 1]) {
			fprintf(stderr, "Invalid memory size format!\n");
			return 0;
		}

		ch = toupper(val[len - 1]);
		switch(ch) {
		case 'G':
			memsize_M *= 1024;
			break;
		case 'M':
			break;
		default:
			fprintf(stderr, "Invalid suffix for memory size!\n");
			return 0;
		}
	}

	/* Get free hugepages */
	fd = open("/sys/kernel/mm/hugepages/hugepages-2048kB/free_hugepages", O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "open /sys/kernel/mm/hugepages/hugepages-2048kB/free_hugepages failed, errno=%d\n", errno);
		return 0;
	}

	n = read(fd, buf, sizeof(buf));
	if (n == -1) {
		fprintf(stderr, "read /sys/kernel/mm/hugepages/hugepages-2048kB/free_hugepages failed, errno=%d\n", errno);
		close(fd);
		return 0;
	}
	close(fd);
	free_hugepg = strtoul(buf, NULL, 10);
	if (free_hugepg >= memsize_M/2)
		return memsize_M;

	/* Get nr hugepages */
	fd = open("/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages", O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "open /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages failed, errno=%d\n", errno);
		return 0;
	}

	n = read(fd, buf, sizeof(buf));
	if (n == -1) {
		fprintf(stderr, "read /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages failed, errno=%d\n", errno);
		close(fd);
		return 0;
	}
	close(fd);
	nr_hugepg = strtoul(buf, NULL, 10);

	required_hugepg = nr_hugepg - free_hugepg + (memsize_M/2);

	snprintf(buf, sizeof(buf), "%d", required_hugepg);
	if (write_to_file("/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages", buf)) {
		fprintf(stderr, "Failed to write /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages!\n");
		return 0;
	}


	while (nr_hugepg != required_hugepg) {
		fd = open("/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages", O_RDONLY);
		if (fd == -1) {
			fprintf(stderr, "open /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages failed, errno=%d\n", errno);
			return 0;
		}

		n = read(fd, buf, sizeof(buf));
		if (n == -1) {
			fprintf(stderr, "read /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages failed, errno=%d\n", errno);
			close(fd);
			return 0;
		}
		close(fd);
		nr_hugepg = strtoul(buf, NULL, 10);
		if (wait_cnt < 200) {
			usleep(10000);
			wait_cnt++;
		} else {
			fprintf(stderr, "Hugepage cannot achieve required size!\n");
			return 0;
		}
	}
	return memsize_M;
}

static int set_available_vf(void)
{
	int fd = 0;
	int total_vfs = 0;
	int dev_id = 0;
	ssize_t n = 0;
	char buf[64] = { 0 };
	int i;
	int ret = 0;

	fd = open(INTEL_GPU_DEV_PATH"/sriov_totalvfs", O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "open %s/sriov_totalvfs failed, errno=%d\n", INTEL_GPU_DEV_PATH, errno);
		return 0;
	}

	n = read(fd, buf, sizeof(buf));
	if (n == -1) {
		fprintf(stderr, "read %s/sriov_totalvfs failed, errno=%d\n", INTEL_GPU_DEV_PATH, errno);
		close(fd);
		return 0;
	}
	close(fd);
	total_vfs = strtoul(buf, NULL, 10);
	/* Limit total VFs to conserve memory */
	total_vfs = total_vfs > 4 ? 4 : total_vfs;

	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "%d", total_vfs);

	if (write_to_file(INTEL_GPU_DEV_PATH"/sriov_drivers_autoprobe", "0")) {
		fprintf(stderr, "Unable to de-probe sriov drivers");
		return -1;
	}

	if (write_to_file("/sys/class/drm/card0/device/sriov_numvfs", buf)) {
		fprintf(stderr, "Unable to de-probe sriov drivers");
		return -1;
	}

	if (write_to_file(INTEL_GPU_DEV_PATH"/sriov_drivers_autoprobe", "1")) {
		fprintf(stderr, "Unable to auto-probe sriov drivers");
		return -1;
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
	close(fd);
	dev_id = strtol(buf, NULL, 16);


	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "8086 %x", dev_id);

	/* Create new vfio id for GPU */
	ret = write_to_file(PCI_DRIVER_PATH"vfio-pci/new_id", buf);
	if (ret == EEXIST) {
		if (write_to_file("/sys/bus/pci/drivers/vfio-pci/remove_id", buf)) {
			fprintf(stderr, "Cannot remove original GPU vfio id\n");
			return -1;
		}
		if (write_to_file(PCI_DRIVER_PATH"vfio-pci/new_id", buf)) {
			fprintf(stderr, "Cannot add new GPU vfio id\n");
			return -1;
		}
	} else if (ret != 0) {
		return -1;
	}

	for (i = 1; i < total_vfs; i++) {
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "/sys/bus/pci/devices/0000:00:02.%d/enable", i);
		fd = open(buf, O_RDONLY);
		if (fd == -1) {
			fprintf(stderr, "open %s failed, errno=%d\n", buf, errno);
			return -1;
		}

		memset(buf, 0, sizeof(buf));
		n = read(fd, buf, sizeof(buf));
		if (n == -1) {
			fprintf(stderr, "read %s failed, errno=%d\n", buf, errno);
			close(fd);
			return -1;
		}
		close(fd);
		if (strtol(buf, NULL, 10) == 0) {
			return i;
		}
	}

	return -1;
}

static int setup_sriov(GKeyFile *gkf, char **p, size_t *size)
{
	int cx = 0;
	int monitor_id = -1;
	int vf = 0;
	int mem = 0;
	GError *error = NULL;
	keyfile_group_t *g = NULL;

	g = &g_group[GROUP_VGPU];
	monitor_id = g_key_file_get_integer(gkf, g->name, g->key[VGPU_MON_ID], &error);
	if (error) {
		if (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
			cx = snprintf(*p, *size, " -display gtk,gl=on");
		} else {
			fprintf(stderr, "Invalid monitor id!\n");
			return -1;
		}
	} else {
		cx = snprintf(*p, *size, " -display gtk,gl=on,monitor=%d", monitor_id);
	}
	*p += cx; *size -= cx;

	mem = setup_hugepages(gkf);
	if (mem == 0)
		return -1;
	vf = set_available_vf();

	cx = snprintf(*p, *size, " -device virtio-vga,max_outputs=1,blob=true"
				 " -device vfio-pci,host=0000:00:02.%d"
				 " -object memory-backend-memfd,hugetlb=on,id=mem_sriov,size=%dM"
				 " -machine memory-backend=mem_sriov",
				 vf, mem);
	*p += cx; *size -= cx;

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
	for (int i=0; i < pci_count; i++) {
		fprintf(stderr, "pci unset: %s\n", pci_pt_record[i]);
		setup_passthrough(pci_pt_record[i], 1);
		free(pci_pt_record[i]);
	}
}

static int check_soundcard_on_host(){
        int ret = system("cat /proc/asound/cards | grep sof");
        return ret;
}

static void insert_sof_tgl_snd_module() {
	int ret = 0;
	if ((ret = load_kernel_module("snd-sof-pci-intel-tgl")) != 0)
		fprintf(stderr, "errro loading snd-sof module\n");
}

static void cleanup(int num, int removed_sof_tgl_snd_module)
{
	(void)num;
	fprintf(stderr, "Cleaning up...\n");
	cleanup_child_proc();
	cleanup_rpmb();
	cleanup_passthrough();
	if(removed_sof_tgl_snd_module)
		insert_sof_tgl_snd_module();

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

	char iommu_grp_dev[128] = {0};
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
		char *grp_dev = ep->d_name;
		if (strcmp(grp_dev, ".") == 0 || strcmp(grp_dev, "..") == 0) {
			continue;
		}
		char buffer[512] = {0}, device[64] = {0}, vendor[64] = {0};
			
		/* Get device_id and vendor_id */
		snprintf(buffer, 512, "%s/%s/device", iommu_grp_dev, grp_dev);
		FILE *f = fopen(buffer, "r");
		if (f == NULL)
			continue;
		if (!fgets(device, sizeof(device), f)) {
			fprintf(stderr, "Cannot get device name for device %s.", grp_dev);
			fclose(f);
			continue;
		}
		fclose(f);

		memset(buffer, 0, sizeof(buffer));
		snprintf(buffer, 512, "%s/%s/vendor", iommu_grp_dev, grp_dev);
		f = fopen(buffer, "r");
		if (f == NULL)
			continue;
		if (!fgets(vendor, sizeof(vendor), f)) {
			fprintf(stderr, "Cannot get vendor name for device %s.", grp_dev);
			fclose(f);
			continue;
		}
		fclose(f);

		if (unset) {
			memset(buffer, 0, sizeof(buffer));
			snprintf(buffer, 512, "%s/%s/driver", iommu_grp_dev, grp_dev);

			if (is_vfio_driver(buffer) == 0) {
				memset(buffer, 0, sizeof(buffer));
				snprintf(buffer, sizeof(buffer), "%.6s %.6s", vendor, device);
				if (write_to_file("/sys/bus/pci/drivers/vfio-pci/remove_id", buffer)) {
					fprintf(stderr, "Cannot remove original GPU vfio id\n");
					return -1;
				}
				/* Unbind original driver */
				if (write_to_file("/sys/bus/pci/drivers/vfio-pci/unbind", grp_dev)) {
					fprintf(stderr, "%s: Cannot unbind original driver!\n", __func__);
				}
			}

			if (check_driver_unbinded(grp_dev) == 0) {
				fprintf(stderr, "drivers_probe reset %s\n", grp_dev);
				write_to_file("/sys/bus/pci/drivers_probe", grp_dev);
			} else {
				fprintf(stderr, "Failed to unbind driver or did not finish in the given time frame.\n");
			}
		} else {
			/* Check if driver exists, unbind if it exists */
			memset(buffer, 0, sizeof(buffer));
			snprintf(buffer, 512, "%s/%s/driver", iommu_grp_dev, grp_dev);
			DIR* dir = opendir(buffer);
			if (dir) {
				/* Driver directory exists. */
				if (is_vfio_driver(buffer) == 0) {
					memset(buffer, 0, sizeof(buffer));
					snprintf(buffer, sizeof(buffer), "%.6s %.6s", vendor, device);
					write_to_file("/sys/bus/pci/drivers/vfio-pci/remove_id", buffer);
				}
				memset(buffer, 0, sizeof(buffer));
				snprintf(buffer, 512, "%s/%s/driver/unbind", iommu_grp_dev, grp_dev);
				write_to_file(buffer, grp_dev);

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

static void setup_vnet(GKeyFile *gkf, char **p, size_t *size) {
	int cx = 0;
	keyfile_group_t *g = NULL;
	g_autofree gchar *val = NULL;
	g_autofree gchar *val1 = NULL;
	char buf[512] = { 0 };
	int bs = sizeof(buf);

	g = &g_group[GROUP_VNET];

	val = g_key_file_get_string(gkf, g->name, g->key[VNET_ADB_PORT], NULL);
	val1 = g_key_file_get_string(gkf, g->name, g->key[VNET_FASTBOOT_PORT], NULL);
	if (val && val1) {
		cx = snprintf(buf, bs, " -netdev user,id=net0,hostfwd=tcp::%s-:5555,hostfwd=tcp::%s-:5554", val, val1);
	} else if (val) {
		cx = snprintf(buf, bs, " -netdev user,id=net0,hostfwd=tcp::%s-:5555", val);
	} else if (val1) {
		cx = snprintf(buf, bs, " -netdev user,id=net0,hostfwd=tcp::%s-:5554", val1);
	} else {
		cx = snprintf(buf, bs, " -netdev user,id=net0");
	}

	if (cx < 0 || cx >= bs)
		return;

	bs -= cx;

	val = g_key_file_get_string(gkf, g->name, g->key[VNET_MODEL], NULL);
	if (val == NULL) {
		cx = snprintf(buf + cx, bs, " -device e1000,netdev=net0,bus=pcie.0,addr=0xA");
	} else {
		if (strcmp(val, "none") == 0) {
			fprintf(stderr, "Disable ethernet emulation!\n");
			return;
		} else {
			cx = snprintf(buf + cx, bs, " -device %s,netdev=net0,bus=pcie.0,addr=0xA", val);
		}
	}
	if (cx < 0 || cx >= bs)
		return;

	bs -= cx;

	strncpy(*p, buf, sizeof(buf) - bs);
	*p += (sizeof(buf) - bs);
	*size -= (sizeof(buf) - bs);
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
	char buffer[1024] = {0};
	g_strstrip(path);
	snprintf(buffer, 1023, "/tmp/%s", pipe);
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
	char sock_path[1024] = {0};
	char buffer[2048] = {0};
	gchar **cmd = NULL;

	if (!path || !p)
		return 0;

	g_strstrip(path);
	cmd = g_strsplit(path, " ", 2);

	snprintf(sock_path, 1023, "/tmp/%s", socket);
	snprintf(buffer, 2047, "%s %s", sock_path, cmd[1]);

	fprintf(stderr, "PM Control: %s %s\n", cmd[0], buffer);
	int ret = execute_cmd(cmd[0], buffer, strlen(buffer), 1);
	if (ret == 0) {
		cx = snprintf(p, size, " -qmp unix:%s,server=on,wait=off -no-reboot", sock_path);
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

/* strip parameters if it is already set in previouly set cmd */
static void strip_duplicate(gchar *val, const gchar *inner_cmd)
{
	gchar *to_split = NULL;
	gchar **sub_str = NULL;
	guint i;

	if ((!val) || (!inner_cmd))
		return;

	to_split = g_strconcat(" ", val, NULL);
	sub_str = g_strsplit(val, " -", 0);

	for (i = 0; sub_str[i]; i++) {
		gchar *find = g_strstr_len(inner_cmd, -1, sub_str[i]);
		if (find) {
			gchar *start = g_strstr_len(val, -1, sub_str[i]);
			memset(start, ' ', strlen(sub_str[i]));
			*(start - 1) = ' '; //set '-' to ' '
			g_strstrip(start - 1);
		}
	}

	if (sub_str)
		g_strfreev(sub_str);
	if (to_split)
		g_free(to_split);
}

int start_guest(char *name)
{
	int ret = 0;
	int removed_sof_tgl_snd_module = 0;
	int cx, uid;
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

	val = g_key_file_get_string(gkf, g->name, g->key[GLOB_VSOCK_CID], NULL);
	if (val) {
		cx = snprintf(p, size, " -device vhost-vsock-pci,id=vhost-vsock-pci0,guest-cid=%s,bus=pcie.0,addr=0x4", val);
	} else {
		cx = snprintf(p, size, " -device vhost-vsock-pci,id=vhost-vsock-pci0,guest-cid=3,bus=pcie.0,addr=0x4");
	}
	p += cx; size -= cx;

	setup_vnet(gkf, &p, &size);

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
		val = g_key_file_get_string(gkf, g->name, g->key[AAF_AUDIO_TYPE], NULL);
		if (val) {
			if (0 == strcmp(val, AUDIO_TYPE_HDA_STR))
				set_aaf_option(AAF_CONFIG_AUDIO,  AAF_AUDIO_TYPE_HDA);
			else if (0 == strcmp(val, AUDIO_TYPE_SOF_STR)) {
				set_aaf_option(AAF_CONFIG_AUDIO,  AAF_AUDIO_TYPE_SOF);
			} else
				g_warning("Invalid setting of AAF set audio type option, it should be either hda or sof\n");
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
		if (!check_soundcard_on_host()) {
			remove_sof_tgl_snd_module();
			removed_sof_tgl_snd_module = 1;
                }
		if (passthrough_gpu())
			return -1;
		cx = snprintf(p, size, " -vga none -nographic -device vfio-pci,host=00:02.0,x-igd-gms=2,id=hostdev0,bus=pcie.0,addr=0x2,x-igd-opregion=on");
		p += cx; size -= cx;
		set_aaf_option(AAF_CONFIG_GPU_TYPE, AAF_GPU_TYPE_GVTD);
	} else if (strcmp(val, VGPU_OPTS_VIRTIO_STR) == 0) {
		cx = snprintf(p, size, " -display gtk,gl=on -device virtio-vga-gl");
		p += cx; size -= cx;
		set_aaf_option(AAF_CONFIG_GPU_TYPE, AAF_GPU_TYPE_VIRTIO);
	} else if (strcmp(val, VGPU_OPTS_RAMFB_STR) == 0) {
		cx = snprintf(p, size, " -display gtk,gl=on -device ramfb");
		p += cx; size -= cx;
	} else if (strcmp(val, VGPU_OPTS_VIRTIO2D_STR) == 0) {
		cx = snprintf(p, size, " -display gtk,gl=on -device virtio-vga");
		p += cx; size -= cx;
		set_aaf_option(AAF_CONFIG_GPU_TYPE, AAF_GPU_TYPE_VIRTIO);
	} else if (strcmp(val, VGPU_OPTS_SRIOV_STR) == 0) {
		if (setup_sriov(gkf, &p, &size))
			return -1;
		set_aaf_option(AAF_CONFIG_GPU_TYPE, AAF_GPU_TYPE_VIRTIO);
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

	uid = get_uid();
	cx = snprintf(p, size," -device intel-hda -device hda-duplex,audiodev=android_spk -audiodev id=android_spk,timer-period=5000,driver=pa,server=/run/user/%d/pulse/native,in.fixed-settings=off,out.fixed-settings=off" , uid);
	p += cx; size -= cx;
	/* extra GROUP */
	g = &g_group[GROUP_EXTRA];
	/* extra cmds */
	val = g_key_file_get_string(gkf, g->name, g->key[EXTRA_CMD], NULL);
	if (val != NULL && (strcmp("", val) != 0)) {
		strip_duplicate(val, fixed_cmd);
		strip_duplicate(val, cmd_str);
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
	
	cleanup(0, removed_sof_tgl_snd_module);

	if (ret != 0) {
		err(1, "%s:Failed to execute emulator command, err=%d\n", __func__, errno);
		return -1;
	}

	g_key_file_free(gkf);
	return 0;
}
