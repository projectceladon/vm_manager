/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <sys/stat.h>
#include <sys/syslog.h>

#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

#include <string>

#include <boost/filesystem.hpp>

#include "utils/utils.h"
#include "utils/log.h"

static char *civ_config_path = NULL;

const char *GetConfigPath(void) {
    if (civ_config_path) {
        return civ_config_path;
    }

    int ret = 0;
    char *sudo_uid = NULL;
    char *sudo_gid = NULL;
    uid_t ruid, euid, suid;
    gid_t rgid, egid, sgid;

    getresuid(&ruid, &euid, &suid);
    getresgid(&rgid, &egid, &sgid);
    suid = geteuid();
    sgid = getegid();

    sudo_uid = std::getenv("SUDO_UID");
    sudo_gid = std::getenv("SUDO_GID");

    if (sudo_gid) {
        egid = atoi(sudo_gid);
        setresgid(rgid, egid, sgid);
    }

    if (sudo_uid) {
        euid = atoi(sudo_uid);
        setresuid(ruid, euid, suid);
    }

    civ_config_path = new char[MAX_PATH];
    memset(civ_config_path, 0, MAX_PATH);

    struct passwd pwd;
    struct passwd *ppwd = &pwd;
    struct passwd *presult = NULL;
    char buffer[4096];
    ret = getpwuid_r(euid, ppwd, buffer, sizeof(buffer), &presult);
    if (ret != 0 || presult == NULL)
        return NULL;

    boost::system::error_code ec;
    snprintf(civ_config_path, MAX_PATH, "%s%s", pwd.pw_dir, "/.intel/.civ");
    if (!boost::filesystem::exists(civ_config_path, ec)) {
        if (!boost::filesystem::create_directories(civ_config_path, ec)) {
            delete[] civ_config_path;
            civ_config_path = NULL;
        }
    }

    if (sudo_gid)
        setresgid(rgid, sgid, egid);

    if (sudo_uid)
        setresuid(ruid, suid, euid);

    return civ_config_path;
}

int Daemonize(void) {
    if (pid_t pid = fork()) {
        if (pid > 0) {
            exit(0);
        } else {
            LOG(error) << "First fork failed %m!";
            return -1;
        }
    }

    setsid();
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    chdir("/");

    if (pid_t pid = fork()) {
        if (pid > 0) {
            return pid;
        } else {
            LOG(error) << "Second fork failed!";
            exit(-1);
        }
    }

    umask(0);
    for (int t = sysconf(_SC_OPEN_MAX); t >= 0; t--) {
        close(t);
    }

    /* Close the standard streams. */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    int fd = open("/dev/null", O_RDWR);
    if (fd < 0)
        return -1;

    if (fd != STDIN_FILENO) {
        close(fd);
        return -1;
    }
    if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
        return -1;
    if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
        return -1;

    return 0;
}

