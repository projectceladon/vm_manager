/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <sys/socket.h>
#include <linux/vm_sockets.h>

#include <iostream>
#include <string>

#include <boost/program_options.hpp>
#include <grpcpp/grpcpp.h>

#include "../../include/constants/vm_manager.h"
#include "vm_guest.grpc.pb.h"

class CivAppLauncherClient {
 public:
    explicit CivAppLauncherClient(std::shared_ptr<grpc::Channel> channel)
        : stub_(vm_manager::CivAppLauncher::NewStub(channel)) {}

    bool LaunchApp(std::string app_name, uint32_t disp_id) {
        vm_manager::AppLaunchRequest request;
        request.set_app_name(app_name);
        request.set_disp_id(disp_id);

        vm_manager::AppLaunchResponse reply;

        grpc::ClientContext context;

        grpc::Status status = stub_->LaunchApp(&context, request, &reply);

        if (status.ok()) {
            std::cout << "Replay code: " << reply.code() << ", status=" << reply.status() << std::endl;
            return true;
        } else {
            std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
            return false;
        }
    }

 private:
    std::unique_ptr<vm_manager::CivAppLauncher::Stub> stub_;
};

namespace po = boost::program_options;
class AppLauncherOptions final {
 public:
    AppLauncherOptions() {
        cmdline_options_.add_options()
            ("help,h",    "Show this help message")
            ("cid,c",  po::value<std::uint32_t>(), "Cid of CiV guest")
            ("name,n", po::value<std::string>(), "Name of APP")
            ("display,d", po::value<std::uint32_t>(), "Display id of CiV guest");
    }

    AppLauncherOptions(AppLauncherOptions &) = delete;
    AppLauncherOptions& operator=(const AppLauncherOptions &) = delete;

    bool ParseOptions(int argc, char* argv[]) {
        po::store(po::command_line_parser(argc, argv).options(cmdline_options_).run(), vm_);
        po::notify(vm_);

        if (vm_.empty()) {
            PrintHelp();
            return false;
        }

        if (vm_.count("help")) {
            PrintHelp();
            return true;
        }

        if (vm_.count("cid") != 1) {
            PrintHelp();
            return false;
        }
        uint32_t cid = vm_["cid"].as<uint32_t>();

        std::string app_name;
        if (vm_.count("name") != 1) {
            PrintHelp();
            return false;
        }
        app_name = vm_["name"].as<std::string>();

        uint32_t disp_id = -1;
        if (vm_.count("display") != 1) {
            PrintHelp();
            return false;
        }
        disp_id = vm_["display"].as<uint32_t>();

        char target_addr[50] = { 0 };
        snprintf(target_addr, sizeof(target_addr) - 1, "vsock:%u:%u",
                cid,
                vm_manager::kCivAppLauncherListenerPort);
        CivAppLauncherClient launcher(grpc::CreateChannel(target_addr, grpc::InsecureChannelCredentials()));
        if (launcher.LaunchApp(app_name, disp_id))
            return true;

        return false;
    }

 private:
    void PrintHelp(void) {
        std::cout << "Usage:\n";
        std::cout << "  app-launcher-client"
                  << " [-c cid] [-n app_name] [-d display_id]\n";
        std::cout << "Options:\n";

        std::cout << cmdline_options_ << std::endl;
        std::cout << "Example:\n";
        std::cout << " ./app-launcher-client -c 1000 -n \"com.android.settings/.Settings\" -d 2" << std::endl;
    }

    boost::program_options::options_description cmdline_options_;
    boost::program_options::variables_map vm_;
};

int main(int argc, char *argv[]) {
    int ret = -1;
    try {
        AppLauncherOptions o;

        if (o.ParseOptions(argc, argv))
            ret = 0;
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return ret;
}
