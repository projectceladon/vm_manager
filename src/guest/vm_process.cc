/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <fstream>
#include <ctime>

#include <boost/process.hpp>
#include <boost/process/environment.hpp>
#include <boost/thread.hpp>
#include <boost/process/extend.hpp>

#include "utils/log.h"
#include "guest/vm_process.h"

namespace vm_manager {

void VmProcSimple::ThreadMon(void) {
    std::error_code ec;

    time_t rawtime;
    struct tm timeinfo;
    char t_buf[80];
    time(&rawtime);
    localtime_r(&rawtime, &timeinfo);
    strftime(t_buf, 80 , "%Y-%m-%d_%T", &timeinfo);

    boost::process::environment env;
    for (std::string s : env_data_) {
        if (s.length() == 0)
            continue;
        env.set(s.substr(0, s.find('=')), s.substr(s.find('=') + 1));
    }

    LOG(info) << "CMD: " << cmd_;
    size_t exe_pos_begin = cmd_.find_first_not_of(' ');
    if (exe_pos_begin == std::string::npos)
        return;

    size_t exe_pos_end = cmd_.find_first_of(' ', exe_pos_begin);
    if (exe_pos_end == std::string::npos)
        exe_pos_end = cmd_.size();
    std::string exe = cmd_.substr(exe_pos_begin, exe_pos_end - exe_pos_begin);

    std::string tid = boost::lexical_cast<std::string>(mon_->get_id());

    std::string f_out = log_dir_ + std::string(basename(exe.c_str())) + "_" + t_buf + "_" + tid + "_out.log";
    std::ofstream fo(f_out);
    fo.close();

    boost::filesystem::permissions(f_out, boost::filesystem::perms::owner_read |
                                          boost::filesystem::perms::owner_write |
                                          boost::filesystem::perms::group_read |
                                          boost::filesystem::perms::group_write |
                                          boost::filesystem::perms::others_read |
                                          boost::filesystem::perms::others_write |
                                          boost::filesystem::add_perms);

    c_ = std::make_unique<boost::process::child>(
        cmd_,
        boost::process::env = env,
        (boost::process::std_out & boost::process::std_err) > f_out,
        ec,
        boost::process::extend::on_success = [this](auto & exec) {
            child_latch_.count_down();
        },
        boost::process::extend::on_error = [this](auto & exec, const std::error_code& ec) {
            child_latch_.count_down();
    });

    c_->wait(ec);

    std::ofstream out(f_out, std::fstream::app);
    out << "\n\nCMD: " << cmd_;

    int result = c_->exit_code();

    LOG(info) << "Thread-0x" << tid << " Exiting"
              << "\n\t\tChild-" << c_->id() << " exited, exit code=" << result
              << "\n\t\tlog: " << f_out;
}

void VmProcSimple::Run(void) {
    mon_ = std::make_unique<boost::thread>([this] { ThreadMon(); });
    child_latch_.wait();
}

void VmProcSimple::Join(void) {
    if (!mon_)
        return;
    if (mon_->joinable())
        mon_->join();
}

void VmProcSimple::SetEnv(std::vector<std::string> env) {
    env_data_ = env;
}

void VmProcSimple::SetLogDir(const char *path) {
    if (!path)
        return;

    boost::filesystem::path p(path);
    boost::system::error_code ec;
    if (boost::filesystem::exists(p, ec)) {
        if (!boost::filesystem::is_directory(p, ec))
            return;
    } else {
        if (!boost::filesystem::create_directories(p, ec))
            return;
    }

    log_dir_.assign(boost::filesystem::absolute(p, ec).c_str()).append("/");
}

void VmProcSimple::Stop(void) {
    try {
        if (!mon_ || !c_)
            return;

        if (!mon_.get())
            return;

        if (!c_->running())
            return;

        LOG(info) << "Terminate CoProc: " << c_->id();
        kill(c_->id(), SIGTERM);
        mon_->try_join_for(boost::chrono::seconds(10));
        mon_.reset(nullptr);
    } catch (std::exception& e) {
        LOG(error) << "Exception: " << e.what();
    }
}

bool VmProcSimple::Running(void) {
    if (c_)
        return c_->running();
    return false;
}

VmProcSimple::~VmProcSimple() {
    VmProcSimple::Stop();
}


void VmCoProcRpmb::Run(void) {
    LOG(info) << bin_ << " " << data_dir_;
    boost::system::error_code bec;
    if (!boost::filesystem::exists(data_dir_ + "/" + kRpmbData, bec)) {
        std::error_code ec;
        boost::process::child init_data(bin_ + " --dev " + data_dir_ + "/" + kRpmbData + " --init --size 2048");
        init_data.wait(ec);
        int ret = init_data.exit_code();
    }
    cmd_ = bin_ + " --dev " + data_dir_ + "/" + kRpmbData + " --sock " + sock_file_;

    VmProcSimple::Run();
}

void VmCoProcRpmb::Stop(void) {
    VmProcSimple::Stop();

    /* TODO: this temp rpmb sock file is created by rpmb_dev, but rpmb_dev did not cleanup
             when exit, so here check and remove the sock file after the rpmb_dev process
             exited. Need to remove this block of code once the rpmb_dev fixed the issue.
     */
    boost::system::error_code bec;
    if (boost::filesystem::exists(sock_file_, bec)) {
        LOG(info) << "Cleanup Rpmb CoProc stuff ...";
        boost::filesystem::remove(sock_file_);
    }
}


VmCoProcRpmb::~VmCoProcRpmb() {
    VmCoProcRpmb::Stop();
}

void VmCoProcVtpm::Run(void) {
    LOG(info) << bin_ << " " << data_dir_;

    boost::system::error_code bec;
    if (!boost::filesystem::exists(data_dir_, bec)) {
        LOG(warning) << "Data path for Vtpm not exists!";
        return;
    }

    cmd_ = bin_ + " socket --tpmstate dir=" + data_dir_ +
           " --tpm2 --ctrl type=unixio,path=" + data_dir_ + "/" + kVtpmSock;

    VmProcSimple::Run();
}

void VmCoProcVtpm::Stop(void) {
    VmProcSimple::Stop();
}

VmCoProcVtpm::~VmCoProcVtpm() {
    VmCoProcVtpm::Stop();
}

}  //  namespace vm_manager
