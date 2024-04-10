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

// Global string variables for each component
const std::string INTEL_VID = "8087:";
const std::string INTEL_PID_8265 = "0a2b";
const std::string INTEL_PID_3168 = "0aa7";
const std::string INTEL_PID_9260 = "0025";
const std::string INTEL_PID_9560 = "0aaa";
const std::string INTEL_PID_AX201 = "0026";
const std::string INTEL_PID_AX211 = "0033";
const std::string INTEL_PID_AX210 = "0032";

static char *civ_config_path = NULL;
std::string runCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

bool extractBusDevice(const std::string& input, std::string& bus, std::string& device) {
    std::istringstream iss(input);
    std::string token;
    if (iss >> token >> bus >> token >> device >> token) {
        size_t colonPos = device.find(':');
        if (colonPos != std::string::npos) {
            device.erase(colonPos);
        }
        return true;
    }

    return false;
}

std::string getBluetoothAddress(std::string output) {
    std::string bus, device;
    if (extractBusDevice(output, bus, device)) {

        std::string udevCommand = "udevadm info -a -p $(udevadm info -q path -n /dev/bus/usb/"
        + bus + "/" + device + ") | ""grep \"/devices/pci0000:\" | "
                                   "sed -n \"s|.*'/devices/pci0000:\\(.*\\)/usb\\(.*\\)|\\1|p\"";
        // Run the constructed command and get its output
        std::string udevOutput = runCommand(udevCommand.c_str());
        LOG(warning) << "udevadm command output:\n" << udevOutput.substr(3,12) << std::endl;
        return (udevOutput.substr(3,12));
    } else {
        LOG(warning) << "Could not extract bus and device numbers from the output." << std::endl;
        return "";
    }
}
std::string checkIntelDeviceId() {
    std::string command = "lsusb | grep -E '" + INTEL_VID +
                          "(" + INTEL_PID_8265 + "|" + INTEL_PID_3168 + "|" +
                          INTEL_PID_9260 + "|" + INTEL_PID_9560 + "|" +
                          INTEL_PID_AX201 + "|" + INTEL_PID_AX211 + "|" +
                          INTEL_PID_AX210 + ")'";
    // Get the command output//
    std::string output = runCommand(command.c_str());
    if (!output.empty()) {
        LOG(warning) << "Intel BT-Controller found!" << std::endl;
        return (getBluetoothAddress(output));
    }
    else {
        LOG(warning) << "Intel BT-Controller not found!" << std::endl;
    }
    return "";
}

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

