/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <sys/syslog.h>
#include <iostream>
#include <filesystem>
#include <map>
#include <string>

#include <boost/program_options.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/process/environment.hpp>

#include "utils/log.h"
#include "utils/utils.h"
#include "guest/vm_builder.h"
#include "guest/tui.h"
#include "services/server.h"
#include "services/client.h"

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_MICRO 0

namespace vm_manager {

bool IsServerRunning() {
    try {
        Client c;
        c.Notify(kCivMsgTest);
        return true;
    } catch (std::exception& e) {
        // LOG(warning) << "Server is not running: " << e.what();
        return false;
    }
}

static void ListGuest(void) {
    if (!IsServerRunning()) {
        LOG(info) << "server is not running! Please start server!";
        return;
    }

    Client c;
    if (!c.Notify(kCivMsgListVm)) {
        LOG(error) << "List guest: " << " Failed!";
        return;
    }
    auto vm_list = c.GetGuestLists();
    std::cout << "=====================" << std::endl;
    for (auto it : vm_list) {
        std::cout << it << std::endl;
    }
    return;
}

static int GetGuestState(std::string name) {
    if (name.empty())
        return -1;
    if (!IsServerRunning()) {
        LOG(info) << "server is not running! Please start server!";
        return -1;
    }

    Client c;
    if (!c.Notify(kCivMsgListVm)) {
        LOG(error) << "List guest: " << " Failed!";
    }
    auto vm_list = c.GetGuestLists();
    for (auto it : vm_list) {
        std::vector<std::string> sp;
        boost::split(sp, it, boost::is_any_of(":"));
        if (sp.size() != 2)
            continue;
        if (sp[0].compare(name) == 0) {
            for (auto i = 0; i < VmBuilder::kVmUnknown; i++) {
                if (sp[1].compare(VmStateToStr(static_cast<VmBuilder::VmState>(i))) == 0) {
                    return i;
                }
            }
        }
    }
    return VmBuilder::kVmUnknown;
}

static void StartGuest(std::string path) {
    if (!IsServerRunning()) {
        LOG(info) << "server is not running! Please start server!";
        return;
    }

    boost::system::error_code ec;
    boost::filesystem::path p(path);

    if (!boost::filesystem::exists(p, ec)) {
        p.clear();
        p.assign(GetConfigPath() + std::string("/") + path + ".ini");
        if (!boost::filesystem::exists(p, ec)) {
            LOG(error) << "CiV config not exists: " << path;
            return;
        }
    }

    Client c;
    c.PrepareStartGuestClientShm(p.c_str());
    if (c.Notify(kCivMsgStartVm)) {
        LOG(info) << "Start guest: " << path << " Done.";
    } else {
        LOG(error) << "Start guest: " << path << " Failed!";
    }
    return;
}

static void StopGuest(std::string name) {
    if (!IsServerRunning()) {
        LOG(info) << "server is not running! Please start server!";
        return;
    }

    Client c;
    c.PrepareStopGuestClientShm(name.c_str());
    if (c.Notify(kCivMsgStopVm)) {
        LOG(info) << "Stop guest: " << name << " Done.";
    } else {
        LOG(error) << "Stop guest: " << name << " Failed!";
    }
    return;
}

static void StartServer(bool daemon) {
    if (IsServerRunning()) {
        LOG(info) << "Server already running!";
        return;
    }

    if (daemon) {
        const char *log_file = "/tmp/civ_server.log";
        int ret = Daemonize();
        if (ret > 0) {
            LOG(info) << "Starting service as daemon (PID=" << ret << ")";
            LOG(info) << "Log will be redirected to " << log_file;
            return;
        } else if (ret < 0) {
            LOG(error) << "vm-manager: failed to Daemonize\n";
            return;
        }

        logger::log2file(log_file);
        LOG(info) << "\n--------------------- "
                  << "CiV VM Manager Service started in background!"
                  << "(PID=" << getpid() << ")"
                  << " ---------------------";
    }

    Server &srv = Server::Get();

    LOG(info) << "Starting Server!";
    srv.Start();

    return;
}

static void StopServer(void) {
    if (!IsServerRunning()) {
        LOG(info) << "server is not running! Please start server first!";
        return;
    }

    Client c;
    c.Notify(kCiVMsgStopServer);
}

namespace po = boost::program_options;
class CivOptions final {
 public:
    CivOptions() {
        cmdline_options_.add_options()
            ("help,h",    "Show ths help message")
            ("create,c",  po::value<std::string>(), "Create a CiV guest")
            ("delete,d",  po::value<std::string>(), "Delete a CiV guest")
            ("start,b",   po::value<std::string>(), "Start a CiV guest")
            ("stop,q",    po::value<std::string>(), "Stop a CiV guest")
            ("flash,f",   po::value<std::string>(), "Flash a CiV guest")
            ("update,u",  po::value<std::string>(), "Update an existing CiV guest")
            ("list,l",    "List existing CiV guest")
            ("version,v", "Show CiV vm-manager version")
            ("start-server",  "Start host server")
            ("stop-server",  "Stop host server")
            ("daemon", "start server as a daemon");
    }

    CivOptions(CivOptions &) = delete;
    CivOptions& operator=(const CivOptions &) = delete;

    void ParseOptions(int argc, char* argv[]) {
        po::store(po::command_line_parser(argc, argv).options(cmdline_options_).run(), vm_);
        po::notify(vm_);

        if (vm_.empty()) {
            PrintHelp();
            return;
        }

        if (vm_.count("help")) {
            PrintHelp();
            return;
        }

        if (vm_.count("create")) {
            CivTui ct;
            ct.InitializeUi();
            return;
        }

        if (vm_.count("delete")) {
            return;
        }

        if (vm_.count("start")) {
            StartGuest(vm_["start"].as<std::string>());
            return;
        }

        if (vm_.count("stop")) {
            StopGuest(vm_["stop"].as<std::string>());
            return;
        }

        if (vm_.count("flash")) {
            return;
        }

        if (vm_.count("update")) {
            return;
        }

        if (vm_.count("list")) {
            ListGuest();
            return;
        }

        if (vm_.count("start-server")) {
            bool daemon = (vm_.count("daemon") == 0) ? false : true;
            StartServer(daemon);
            return;
        }

        if (vm_.count("stop-server")) {
            StopServer();
            return;
        }

        if (vm_.count("version")) {
            PrintVersion();
            return;
        }
    }

 private:
    void PrintHelp(void) {
        std::cout << "Usage:\n";
        std::cout << "  vm-manager"
                  << " [-c] [-d vm_name] [-b vm_name] [-q vm_name] [-f vm_name] [-u vm_name]"
                  << " [-l] [-v] [-h]\n";
        std::cout << "Options:\n";

        std::cout << cmdline_options_ << std::endl;
    }
    void PrintVersion(void) {
        std::cout << "CiV vm-manager version: "
                  << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_MICRO
                  << std::endl;
    }

    boost::program_options::options_description cmdline_options_;
    boost::program_options::variables_map vm_;
};

}  // namespace vm_manager

int main(int argc, char *argv[]) {
    int c;
    int ret = 0;

    logger::init();

    if (!GetConfigPath())
        return -1;

    // set_cleanup();

    vm_manager::CivOptions co;

    co.ParseOptions(argc, argv);

    return ret;
}
