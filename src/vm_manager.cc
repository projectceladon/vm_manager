/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <sys/syslog.h>
#include <iostream>
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
#include "guest/vm_flash.h"
#include "guest/tui.h"
#include "services/server.h"
#include "services/client.h"
#include "revision.h"

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

static bool ListGuest(void) {
    if (!IsServerRunning()) {
        LOG(info) << "server is not running! Please start server!";
        return false;
    }

    Client c;
    if (!c.Notify(kCivMsgListVm)) {
        LOG(error) << "List guest: " << " Failed!";
        return false;
    }
    auto vm_list = c.GetGuestLists();
    std::cout << "=====================" << std::endl;
    for (auto& it : vm_list) {
        std::cout << it << std::endl;
    }
    return true;
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
    for (auto& it : vm_list) {
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

static bool StartGuest(std::string path) {
    if (!IsServerRunning()) {
        LOG(info) << "server is not running! Please start server!";
        return false;
    }

    boost::system::error_code ec;
    boost::filesystem::path p(path);

    if (!boost::filesystem::exists(p, ec) || !boost::filesystem::is_regular_file(p, ec)) {
        p.clear();
        p.assign(GetConfigPath() + std::string("/") + path + ".ini");
        if (!boost::filesystem::exists(p, ec)) {
            LOG(error) << "CiV config not exists: " << path;
            return false;
        }
    }

    Client c;
    c.PrepareStartGuestClientShm(p.c_str());
    if (!c.Notify(kCivMsgStartVm)) {
        LOG(error) << "Start guest: " << path << " Failed!";
        return false;
    }
    LOG(info) << "Start guest: " << path << " Done.";
    return true;
}

static bool StopGuest(std::string name) {
    if (!IsServerRunning()) {
        LOG(info) << "server is not running! Please start server!";
        return false;
    }

    Client c;
    c.PrepareStopGuestClientShm(name.c_str());
    if (!c.Notify(kCivMsgStopVm)) {
        LOG(error) << "Stop guest: " << name << " Failed!";
        return false;
    }
    LOG(info) << "Stop guest: " << name << " Done.";
    return true;
}


static bool GetGuestCid(std::string name) {
    if (!IsServerRunning()) {
        LOG(info) << "server is not running! Please start server first!";
        return false;
    }

    Client c;
    CivVmInfo vi = c.GetCivVmInfo(name.c_str());
    if (vi.state == VmBuilder::VmState::kVmUnknown) {
        LOG(error) << "Failed to get guest Cid: " << name;
        return false;
    }
    std::cout << vi.cid << std::endl;
    return true;
}

static bool StartServer(bool daemon) {
    if (IsServerRunning()) {
        LOG(info) << "Server already running!";
        return false;
    }

    if (daemon) {
        const char *log_file = "/tmp/civ_server.log";
        int ret = Daemonize();
        if (ret > 0) {
            LOG(info) << "Starting service as daemon (PID=" << ret << ")";
            LOG(info) << "Log will be redirected to " << log_file;
            return true;
        } else if (ret < 0) {
            LOG(error) << "vm-manager: failed to Daemonize\n";
            return false;
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

    return true;
}

static bool StopServer(void) {
    if (!IsServerRunning()) {
        LOG(info) << "server is not running! Please start server first!";
        return false;
    }

    Client c;
    if (c.Notify(kCiVMsgStopServer)) {
        if (IsServerRunning())
            return false;
        return true;
    }
    return false;
}

namespace po = boost::program_options;
class CivOptions final {
 public:
    CivOptions() {
        cmdline_options_.add_options()
            ("help,h",    "Show this help message")
            ("create,c",  po::value<std::string>(), "Create a CiV guest")
            // ("delete,d",  po::value<std::string>(), "Delete a CiV guest")
            ("start,b",   po::value<std::string>(), "Start a CiV guest")
            ("stop,q",    po::value<std::string>(), "Stop a CiV guest")
            ("flash,f",   po::value<std::string>(), "Flash a CiV guest")
            // ("update,u",  po::value<std::string>(), "Update an existing CiV guest")
            ("get-cid", po::value<std::string>(), "Get cid of a guest")
            ("list,l",    "List existing CiV guest")
            ("version,v", "Show CiV vm-manager version")
            ("start-server",  "Start host server")
            ("stop-server",  "Stop host server")
            ("daemon", "start server as a daemon");
    }

    CivOptions(CivOptions &) = delete;
    CivOptions& operator=(const CivOptions &) = delete;

    bool ParseOptions(const std::vector<std::string> args) {
        try {
            po::store(po::command_line_parser(args).options(cmdline_options_).run(), vm_);
            po::notify(vm_);
        } catch (std::exception& e) {
            std::cout << e.what() << "\n";
            return false;
        }

        if (vm_.empty()) {
            PrintHelp();
            return false;
        }

        if (vm_.count("help")) {
            PrintHelp();
            return true;
        }

        if (vm_.count("version")) {
            PrintVersion();
            return true;
        }

        if (vm_.count("stop-server")) {
            return StopServer();
        }

        if (vm_.count("start-server")) {
            bool daemon = (vm_.count("daemon") == 0) ? false : true;
            return StartServer(daemon);
        } else {
            if (!IsServerRunning()) {
                boost::filesystem::path cmd(args[0]);
                boost::system::error_code ec;
                if (!boost::filesystem::exists(cmd, ec)) {
                    cmd.assign(boost::process::search_path(args[0]));
                }
                if (boost::process::system(cmd, "--start-server", "--daemon")) {
                    LOG(error) << "Failed to start server of vm-manager!";
                    return false;
                }
                int wait_cnt = 0;
                while (!IsServerRunning()) {
                    if (wait_cnt++ > 100) {
                        LOG(error) << "Cannot start server!";
                        return false;
                    }
                    boost::this_thread::sleep_for(boost::chrono::microseconds(1000));
                }
            }
        }

        if (vm_.count("create")) {
            CivTui ct;
            ct.InitializeUi(vm_["create"].as<std::string>());
            return true;
        }

        if (vm_.count("delete")) {
            return true;
        }

        if (vm_.count("start")) {
            return StartGuest(vm_["start"].as<std::string>());
        }

        if (vm_.count("stop")) {
            return StopGuest(vm_["stop"].as<std::string>());
        }

        if (vm_.count("flash")) {
            VmFlasher f;
            return f.FlashGuest(vm_["flash"].as<std::string>());
        }

        if (vm_.count("update")) {
            return false;
        }

        if (vm_.count("get-cid")) {
            return GetGuestCid(vm_["get-cid"].as<std::string>());
        }

        if (vm_.count("list")) {
            return ListGuest();
        }

        return false;
    }

 private:
    void PrintHelp(void) {
        std::cout << "Usage:\n";
        std::cout << "  vm-manager"
                  << " [-c vm_name] [-b vm_name] [-q vm_name] [-f vm_name] [--get-cid vm_name]"
                  << " [-l] [-v] [-h]\n";
        std::cout << "Options:\n";

        std::cout << cmdline_options_ << std::endl;
    }
    void PrintVersion(void) {
        std::cout << "CiV vm-manager version: "
                  << BUILD_REVISION
                  << " " << BUILD_TYPE
                  << " " << BUILD_TIMESTAMP
                  << " @" << BUILD_FQDN
                  << std::endl;
    }

    boost::program_options::options_description cmdline_options_;
    boost::program_options::variables_map vm_;
};

}  // namespace vm_manager

int main(int argc, char *argv[]) {
    int ret = -1;

    try {
        logger::init();

        if (!GetConfigPath())
            return ret;

        vm_manager::CivOptions co;

        std::vector<std::string> args(argv, argv + argc);
        if (args.size() == 2) {
            if (args[1].compare("--afl-fuzz") == 0) {
                // Take input from stdio when fuzzing.
                std::string str;
                std::getline(std::cin, str);
                args.clear();
                boost::split(args, str, boost::is_any_of(" "));
                args.insert(args.begin(), argv[0]);
            }
        }

        if (co.ParseOptions(args)) {
            ret = 0;
        }
    } catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        ret = -1;
    }

    return ret;
}
