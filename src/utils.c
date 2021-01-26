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
#include <sys/param.h>
#include "vm_manager.h"
#include "utils.h"
#include <string.h>
#include <paths.h>
#include <regex.h>

#define PCI_STR_SIZE 16

int load_kernel_module(const char *module) {

	/* First check if module is already loaded */
	FILE * fp;
    char * line = NULL;
    size_t len = 0, name_len = strlen(module);
    ssize_t read;
	// char buffer[1024] = { 0 };

	// snprintf(buffer, 1023, "%s", module);

    fp = fopen("/proc/modules", "r");
    if (fp == NULL) {
		fprintf(stderr, "Cannot read loaded kernel modules.\n");
		return -1;
	}

    while ((read = getline(&line, &len, fp)) != -1) {
		char *temp = strstr(line, module);
		if ((temp == line) && (line[name_len] == '\0')) {
			return 0;
		}
    }

    fclose(fp);
    if (line)
        free(line);
		
	
	int pid;
	int wst;

	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "%s: Failed to fork.", __func__);
		return -1;
	} else if (pid == 0) {
		execlp("modprobe", "modprobe", module, NULL);
		return -1;
	} else {
		wait(&wst);
		if (!(WIFEXITED(wst) && !WEXITSTATUS(wst))) {
			fprintf(stderr, "Failed to load module: %s\n", module);
			return -1;
		}
	}
	return 0;
}


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

	if (arg != NULL) {
		strncpy(arg_dup, arg,sizeof(arg_dup) - 1);

		/* split cmd into argument array */
		while((tok = strtok_r(arg_ptr, " ", &next_tok))){
			argv[i++] = tok;
			arg_ptr = NULL;
		}
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
			fprintf(stderr, "%s: Failed to execute %s %s\n Exit status: %d\n", __func__, cmd, arg, WEXITSTATUS(wst));
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
		tok = strtok_r(NULL," ", &ntok);
	}
	return 0;
}

/* Code adapted from O'Reilly's "Secure Programming Cookbook for C and C++" */

static int orig_ngroups = -1;
static gid_t orig_gid = -1;
static uid_t orig_uid = -1;
static gid_t orig_groups[NGROUPS_MAX];

static void spc_drop_privileges(int permanent)
{
	gid_t newgid = getgid(), oldgid = getegid();
	uid_t newuid = getuid(), olduid = geteuid();

	if (!permanent)
	{
		/* Save information about the privileges that are being dropped so that they
 		* can be restored later.
 		*/
		orig_gid = oldgid;
		orig_uid = olduid;
		orig_ngroups = getgroups(NGROUPS_MAX, orig_groups);
	}

	/* If root privileges are to be dropped, be sure to pare down the ancillary
	* groups for the process before doing anything else because the setgroups(  )
	* system call requires root privileges.  Drop ancillary groups regardless of
	* whether privileges are being dropped temporarily or permanently.
	*/
	if (!olduid)
		setgroups(1, &newgid);

	if (newgid != oldgid)
	{
#if !defined(linux)
		setegid(newgid);
		if (permanent && setgid(newgid) == -1)
			abort();
#else
		if (setregid((permanent ? newgid : -1), newgid) == -1)
			abort();
#endif
	}

	if (newuid != olduid)
	{
#if !defined(linux)
		seteuid(newuid);
		if (permanent && setuid(newuid) == -1)
			abort();
#else
		if (setreuid((permanent ? newuid : -1), newuid) == -1)
			abort();
#endif
	}

	/* verify that the changes were successful */
	if (permanent)
	{
		if (newgid != oldgid && (setegid(oldgid) != -1 || getegid() != newgid))
			abort();
		if (newuid != olduid && (seteuid(olduid) != -1 || geteuid() != newuid))
			abort();
	}
	else
	{
		if (newgid != oldgid && getegid() != newgid)
			abort();
		if (newuid != olduid && geteuid() != newuid)
			abort();
	}
}

static pid_t spc_fork(void)
{
	pid_t childpid;

	if ((childpid = fork()) == -1)
		return -1;

	/* Reseed PRNGs in both the parent and the child */
	/* See Chapter 11 for examples */

	/* If this is the parent process, there's nothing more to do */
	if (childpid != 0)
		return childpid;

	/* This is the child process */
	spc_drop_privileges(1); /* Permanently drop privileges.  See Recipe 1.3 */

	return 0;
}

static SPC_PIPE *spc_popen(const char *path, char *const argv[], char *const envp[])
{
	int stdin_pipe[2], stdout_pipe[2];
	SPC_PIPE *p;

	if (!(p = (SPC_PIPE *)malloc(sizeof(SPC_PIPE))))
		return 0;
	p->read_fd = p->write_fd = 0;
	p->child_pid = -1;

	if (pipe(stdin_pipe) == -1)
	{
		free(p);
		return 0;
	}
	if (pipe(stdout_pipe) == -1)
	{
		close(stdin_pipe[1]);
		close(stdin_pipe[0]);
		free(p);
		return 0;
	}

	if (!(p->read_fd = fdopen(stdout_pipe[0], "r")))
	{
		close(stdout_pipe[1]);
		close(stdout_pipe[0]);
		close(stdin_pipe[1]);
		close(stdin_pipe[0]);
		free(p);
		return 0;
	}
	if (!(p->write_fd = fdopen(stdin_pipe[1], "w")))
	{
		fclose(p->read_fd);
		close(stdout_pipe[1]);
		close(stdin_pipe[1]);
		close(stdin_pipe[0]);
		free(p);
		return 0;
	}

	if ((p->child_pid = spc_fork()) == -1)
	{
		fclose(p->write_fd);
		fclose(p->read_fd);
		close(stdout_pipe[1]);
		close(stdin_pipe[0]);
		free(p);
		return 0;
	}

	if (!p->child_pid)
	{
		/* this is the child process */
		close(stdout_pipe[0]);
		close(stdin_pipe[1]);
		if (stdin_pipe[0] != 0)
		{
			dup2(stdin_pipe[0], 0);
			close(stdin_pipe[0]);
		}
		if (stdout_pipe[1] != 1)
		{
			dup2(stdout_pipe[1], 1);
			close(stdout_pipe[1]);
		}
		execve(path, argv, envp);
		exit(127);
	}

	close(stdout_pipe[1]);
	close(stdin_pipe[0]);
	return p;
}

static int spc_pclose(SPC_PIPE *p)
{
	int status;
	pid_t pid;

	if (p->child_pid != -1)
	{
		do
		{
			pid = waitpid(p->child_pid, &status, 0);
		} while (pid == -1 && errno == EINTR);
	}
	if (p->read_fd)
		fclose(p->read_fd);
	if (p->write_fd)
		fclose(p->write_fd);
	free(p);
	if (pid != -1 && WIFEXITED(status))
		return WEXITSTATUS(status);
	else
		return (pid == -1 ? -1 : 0);
}

int write_to_file(const char *path, const char *buffer)
{
	int n, fd;

	fd = open(path, O_WRONLY);
	if (fd == -1)
	{
		fprintf(stderr, "open %s failed, errno=%d, %s\n", path, errno, strerror(errno));
		return -1;
	}

	n = strlen(buffer);
	if (write(fd, buffer, n) != n)
	{
		fprintf(stderr, "write %s failed, errno=%d, %s\n", path, errno, strerror(errno));
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}

static void print_regerror(int errcode, size_t length, regex_t *compiled)
{
	char buffer[length];
	(void)regerror(errcode, compiled, buffer, length);
	fprintf(stderr, "Regex match failed: %s\n", buffer);
}

int find_pci(const char *name, int n, char *res[])
{
	/* returns number of pci devices found, -1 if error. */
	char buffer[4096];
	char search_key[64];
	regex_t regex;
	int result;
	snprintf(search_key, 64, "....:..:..\\..( %s)", name);
	result = regcomp(&regex, search_key, REG_EXTENDED);
	if (result)
	{
		if (result == REG_ESPACE)
			fprintf(stderr, "%s\n", strerror(ENOMEM));
		else
			fputs("Syntax error in the regular expression passed as first argument\n", stderr);
		return -1;
	}

	regmatch_t matches[1];
	int count_res = 0;
	char *options[] = {"lspci", "-D", (char *)0};
	char *env[] = {(char *)0};
	SPC_PIPE *pipe_lspci = spc_popen("/usr/bin/lspci", options, env);
	if (pipe_lspci != 0)
	{
		while (fgets(buffer, sizeof(buffer), pipe_lspci->read_fd) != NULL)
		{
			result = regexec(&regex, buffer, 1, matches, 0);

			if (!result)
			{
				strncpy(res[count_res], buffer + matches[0].rm_so, 12);
				res[count_res++][12] = '\0';

				if (count_res >= n)
					break;
			}
			else if (result == REG_NOMATCH)
			{
				// No match on current line
			}
			else
			{
				size_t length = regerror(result, &regex, NULL, 0);
				print_regerror(result, length, &regex);
			}
		}
	}
	else
	{
		fprintf(stderr, "Failed to create spc pipe for lspci.");
		return -1;
	}
	spc_pclose(pipe_lspci);
	return count_res;
}

int find_ProgIf(char *pci_device, char *res)
{
	/* returns 0 if ProgIf is found, -1 if not */
	char buffer[4096];
	char search_key[64] = "ProgIf";
	regex_t regex;
	int result;

	result = regcomp(&regex, search_key, REG_EXTENDED);
	if (result)
	{
		if (result == REG_ESPACE)
			fprintf(stderr, "%s\n", strerror(ENOMEM));
		else
			fputs("Syntax error in the regular expression passed as first argument\n", stderr);
		return -1;
	}

	regmatch_t matches[1];
	int count_res = 0;
	char *options[] = {"lspci", "-vmms", pci_device, (char *)0};
	char *env[] = {(char *)0};
	SPC_PIPE *pipe_lspci = spc_popen("/usr/bin/lspci", options, env);
	if (pipe_lspci != 0)
	{
		while (fgets(buffer, sizeof(buffer), pipe_lspci->read_fd) != NULL)
		{
			result = regexec(&regex, buffer, 1, matches, 0);
			if (!result)
			{
				strncpy(res, buffer + matches[0].rm_so + 8, 3);
				res[2] = '\0';
				count_res = 1;
				break;
			}
			else if (result == REG_NOMATCH)
			{
				// No match on current line
			}
			else
			{
				size_t length = regerror(result, &regex, NULL, 0);
				print_regerror(result, length, &regex);
			}
		}
	}
	else
	{
		fprintf(stderr, "Failed to create spc pipe for lspci.");
		return -1;
	}
	return count_res ? 0 : -1;
}
