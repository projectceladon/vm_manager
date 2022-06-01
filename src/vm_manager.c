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
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "vm_manager.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 6
#define VERSION_MICRO 1

#define VERSION xstr(VERSION_MAJOR)"."xstr(VERSION_MINOR)"."xstr(VERSION_MICRO)

char civ_config_path[MAX_PATH] = { 0 };

static void help(int exit_err)
{
	fprintf(stderr,
		"Usage:\n"
		"\tvm-manager [-c] [-i config_file_path] [-d vm_name] [-b vm_name] [-q vm_name] [-f vm_name] [-u vm_name] [-l] [-v] [-h]\n"
		"Options:\n"
		"\t-c, --create\n"
		"\t\tCreate a new CiV guest\n"
		"\t-i, --import\n"
		"\t\tImport a CiV guest from existing config file\n"
		"\t-d, --delete\n"
		"\t\tDelete a CiV guest\n"
		"\t-b, --start\n"
		"\t\tStart a CiV guest\n"
		"\t-q, --stop\n"
		"\t\tStop a CiV guest\n"
		"\t-f, --flash\n"
		"\t\tFlash a CiV guest\n"
		"\t-u, --update\n"
		"\t\tUpdate an existing CiV guest\n"
		"\t-l, --list\n"
		"\t\tList existing CiV guest\n"
		"\t-v, --version\n"
		"\t\tShow CiV vm-manager version \n"
		"\t-h, --help\n"
		"\t\tShow this help message\n"
	);

	if (exit_err)
		exit(EXIT_FAILURE);
}

static int setup_work_dir(void)
{
	struct stat st;
	pid_t pid;
	int ret;
	int wstatus;
	char *suid = NULL;
	int real_uid;

	suid = getenv("SUDO_UID");

	if (suid) {
		real_uid = atoi(suid);
	} else {
		real_uid = getuid();
	}

	snprintf(civ_config_path, MAX_PATH, "%s/.intel/.civ", getpwuid(real_uid)->pw_dir);
	if (stat(civ_config_path, &st) != 0) {
		pid = fork();
		if (pid == 0) {
			ret = execl("/bin/mkdir", "mkdir", "-p", civ_config_path, NULL);
			if (ret == -1) {
				fprintf(stderr, "Failed to execute 'mkdir -p %s'\n", civ_config_path);
				exit(EXIT_FAILURE);
			}
		} else if (pid < 0) {
			fprintf(stderr, "Failed to fork child process to create work dir!\n");
		} else {
			waitpid(pid, &wstatus, 0);
			if (WEXITSTATUS(wstatus) == EXIT_FAILURE)
				return -1;
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int c;
	int ret = 0;

	if (setup_work_dir() != 0)
		return -1;

	set_cleanup();

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "create",  no_argument,       0, 'c' },
			{ "import",  required_argument, 0, 'i' },
			{ "delete",  required_argument, 0, 'd' },
			{ "start",   required_argument, 0, 'b' },
			{ "stop",    required_argument, 0, 'q' },
			{ "flash",   required_argument, 0, 'f' },
			{ "update",  required_argument, 0, 'u' },
			{ "list",    no_argument,       0, 'l' },
			{ "version", no_argument,       0, 'v' },
			{ "help",    no_argument,       0, 'h' },
			{ 0, 0, 0, 0 }
		};

		c = getopt_long(argc, argv, "ci:d:b:q:f:u:lvh", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'c':
			printf("create guest\n");
			ret = create_guest(NULL);
			break;
		case 'i':
			printf("import guest from ini file: %s\n", optarg);
			ret = import_guest(optarg);
			break;
		case 'd':
			printf("remove guest: %s\n", optarg);
			ret = delete_guest(optarg);
			break;
		case 'b':
			printf("start guest %s\n", optarg);
			set_cleanup();
			ret = start_guest(optarg);
			fprintf(stderr, "Exiting.");
			break;
		case 'q':
			printf("stop guest %s\n", optarg);
			ret = stop_guest(optarg);
			break;
		case 'f':
			printf("flash guest %s\n", optarg);
			ret = flash_guest(optarg);
			break;
		case 'u':
			printf("update guest %s\n", optarg);
			ret = create_guest(optarg);
			break;
		case 'l':
			list_guests();
			break;
		case 'v':
			printf("CiV vm-manager version: %s\n", VERSION);
			break;
		case 'h':
			help(0);
			break;

		default:
			help(1);
			break;
		}
	}

	if (optind < argc || argc == 1)
		help(1);

	return ret;
}
