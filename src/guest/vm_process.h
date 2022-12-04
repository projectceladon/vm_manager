/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef SRC_GUEST_VM_PROCESS_H_
#define SRC_GUEST_VM_PROCESS_H_

#include <string>
#include <vector>
#include <memory>

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/thread/latch.hpp>

#include "utils/log.h"

namespace vm_manager {

inline constexpr const char *kRpmbData = "RPMB_DATA";
inline constexpr const char *kRpmbSockPrefix = "/tmp/rpmb_sock_";
inline constexpr const char *kVtpmSock = "swtpm-sock";

class VmProcess {
 public:
    virtual void Run(void) = 0;
    virtual void Stop(void) = 0;
    virtual bool Running(void) = 0;
    virtual void Join(void) = 0;
    virtual void SetLogDir(const char *path) = 0;
    virtual void SetEnv(std::vector<std::string> env) = 0;
    virtual ~VmProcess() = default;
};

class VmProcSimple : public VmProcess {
 public:
    explicit VmProcSimple(std::string cmd) : cmd_(cmd), child_latch_(1) {}
    void Run(void);
    void Stop(void);
    bool Running(void);
    void Join(void);
    void SetEnv(std::vector<std::string> env);
    void SetLogDir(const char *path);
    virtual ~VmProcSimple();

 protected:
    VmProcSimple(const VmProcSimple&) = delete;
    VmProcSimple& operator=(const VmProcSimple&) = delete;

    void ThreadMon(void);


    std::string cmd_;
    std::vector<std::string> env_data_;
    std::string log_dir_ = "/tmp/";

    std::unique_ptr<boost::process::child> c_;
    boost::latch child_latch_;

 private:
    std::unique_ptr<boost::thread> mon_;
};

class VmCoProcRpmb : public VmProcSimple {
 public:
    VmCoProcRpmb(std::string bin, std::string data_dir, std::string sock_file) :
          VmProcSimple(""), bin_(bin), data_dir_(data_dir), sock_file_(sock_file) {}

    void Run(void);
    void Stop(void);
    ~VmCoProcRpmb();

 private:
    VmCoProcRpmb(const VmCoProcRpmb&) = delete;
    VmCoProcRpmb& operator=(const VmCoProcRpmb&) = delete;

    std::string bin_;
    std::string data_dir_;
    std::string sock_file_;
};

class VmCoProcVtpm : public VmProcSimple {
 public:
    VmCoProcVtpm(std::string bin, std::string data_dir) :
          VmProcSimple(""), bin_(bin), data_dir_(data_dir) {}

    void Run(void);
    void Stop(void);
    ~VmCoProcVtpm();

 private:
    VmCoProcVtpm(const VmCoProcVtpm&) = delete;
    VmCoProcVtpm& operator=(const VmCoProcVtpm&) = delete;

    std::string bin_;
    std::string data_dir_;
};

}  //  namespace vm_manager

#endif  // SRC_GUEST_VM_PROCESS_H_
