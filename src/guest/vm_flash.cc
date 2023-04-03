/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <vector>
#include <utility>
#include <memory>

#include <boost/filesystem.hpp>
#include <boost/process.hpp>

#include "guest/vm_flash.h"
#include "guest/config_parser.h"
#include "guest/vm_process.h"
#include "utils/utils.h"
#include "utils/log.h"

namespace vm_manager {

constexpr const char *kVirtualUsbDiskPath("/tmp/flash.vfat");
constexpr const size_t kDdBs = 63_MB;
constexpr const size_t k4GB = 4_GB;

bool VmFlasher::CheckImages(boost::filesystem::path o_dir) {
    boost::system::error_code bec;
    boost::filesystem::path e = boost::process::search_path("split");
    if (e.empty())
        return false;
    std::vector<std::string> args;
    boost::filesystem::directory_iterator end_itr;
    for (boost::filesystem::directory_iterator ditr(o_dir); ditr != end_itr; ++ditr) {
        if (boost::filesystem::is_regular_file(ditr->path(), bec)) {
            total_image_size_ += boost::filesystem::file_size(ditr->path(), bec);
            if (boost::filesystem::file_size(ditr->path(), bec) >= k4GB) {
                args.clear();
                args.push_back("--bytes=" + std::to_string(k4GB - 1));
                args.push_back("--numeric-suffixes");
                args.push_back(ditr->path().string());
                args.push_back(ditr->path().string() + ".part");
                LOG(info) << boost::algorithm::join(args, " ");
                if (boost::process::system(e, boost::process::args(args))) {
                    LOG(error) << "Failed to split: " << ditr->path().string();
                    return false;
                }
                boost::filesystem::remove(ditr->path().string(), bec);
            }
        }
    }
    return true;
}

bool VmFlasher::QemuCreateVirtUsbDisk(void) {
    boost::system::error_code bec;
    boost::filesystem::path file(cfg_.GetValue(kGroupGlob, kGlobFlashfiles));
    if (!boost::filesystem::exists(file, bec)) {
        LOG(error) <<  "Flashfile not exists: " << file.c_str();
        return false;
    }

    if (!boost::filesystem::is_regular_file(file, bec)) {
        LOG(error) <<  "Flashfile is not regular file: " << file.c_str();
        return false;
    }

    if (file.extension().compare(".zip") != 0) {
        virtual_disk_ = file.string();
        return true;
    }

    boost::filesystem::path o_dir("/tmp/" + file.stem().string());
    std::error_code ec;
    std::string cmd;
    if (boost::filesystem::exists(o_dir, bec)) {
        cmd.assign("rm -rf " + o_dir.string());
        LOG(info) << cmd;
        if (boost::process::system(cmd))
            return false;
    }

    cmd.assign("unzip -o " + file.string() + " -d " + o_dir.string());
    LOG(info) << cmd;
    if (boost::process::system(cmd)) {
        LOG(error) << "Failed to unzip: " << file.string();
        return false;
    }

    boost::filesystem::path boot_file(o_dir.string() + "/boot.img");
    if (boost::filesystem::exists(boot_file, bec)) {
        if (!CheckImages(o_dir))
            return false;

        cmd.assign("dd if=/dev/zero of=" + std::string(kVirtualUsbDiskPath) +
                " bs=" + std::to_string(kDdBs) +
                " count=" + std::to_string((total_image_size_ + 1_GB + kDdBs - 1)/kDdBs));
        LOG(info) << cmd;
        if (boost::process::system(cmd)) {
            LOG(error) << "Failed to : " << cmd;
            return false;
        }

        cmd.assign("mkfs.vfat " + std::string(kVirtualUsbDiskPath));
        LOG(info) << cmd;
        if (boost::process::system(cmd)) {
            LOG(error) << "Failed to : " << cmd;
            return false;
        }

        boost::filesystem::directory_iterator end_itr;
        for (boost::filesystem::directory_iterator ditr(o_dir, bec); ditr != end_itr; ++ditr) {
            if (boost::filesystem::is_regular_file(ditr->path(), bec)) {
                cmd.assign("mcopy -o -n -i " + std::string(kVirtualUsbDiskPath) + " " + ditr->path().string() + " ::");
                LOG(info) << cmd;
                if (boost::process::system(cmd)) {
                    LOG(error) << "Failed to : " << cmd;
                    return false;
                }
            }
        }
        cmd.assign("rm -r " + o_dir.string());
        LOG(info) << cmd;
        if (boost::process::system(cmd)) {
            LOG(warning) << "Failed to : " << cmd;
        }

        virtual_disk_ = kVirtualUsbDiskPath;
        return true;
    }

    int count = 0;
    for (auto& p : boost::filesystem::directory_iterator(o_dir, bec)) {
        if (++count >1)
            return false;
    }
    if (count == 1) {
        virtual_disk_ = o_dir.string() + "/" + file.stem().string();
        return true;
    }

    return false;
}

bool VmFlasher::QemuCreateVirtualDisk(void) {
    std::string path = cfg_.GetValue(kGroupDisk, kDiskPath);
    std::string size = cfg_.GetValue(kGroupDisk, kDiskSize);

    std::string cmd("qemu-img create -f qcow2 " + path + " " + size);
    LOG(info) << cmd;
    if (boost::process::system(cmd)) {
        LOG(error) << "Failed to : " << cmd;
        return false;
    }
    return true;
}

bool VmFlasher::FlashWithQemu(void) {
    std::string emul_path = cfg_.GetValue(kGroupEmul, kEmulPath);
    if (emul_path.empty())
        return false;

    if (!QemuCreateVirtUsbDisk())
        return false;

    if (!QemuCreateVirtualDisk()) {
        return false;
    }

    std::string qemu_args(emul_path);

    std::string rpmb_bin = cfg_.GetValue(kGroupRpmb, kRpmbBinPath);
    std::string rpmb_data_dir = cfg_.GetValue(kGroupRpmb, kRpmbDataDir);
    std::string rpmb_data_file = rpmb_data_dir + "/" + std::string(kRpmbData);
    time_t rawtime;
    struct tm timeinfo;
    char t_buf[80];
    time(&rawtime);
    localtime_r(&rawtime, &timeinfo);
    strftime(t_buf, 80 , "%Y-%m-%d_%T", &timeinfo);
    std::string rpmb_sock = std::string(kRpmbSockPrefix) + t_buf;

    std::unique_ptr<VmCoProcRpmb> rpmb_proc;
    boost::system::error_code ec;
    if (!rpmb_bin.empty() && !rpmb_data_dir.empty()) {
        if (boost::filesystem::exists(rpmb_data_file, ec)) {
            if (!boost::filesystem::remove(rpmb_data_file, ec)) {
                LOG(error) << "Failed to remove " << rpmb_data_file;
                LOG(error) << "Please manully remove " << rpmb_data_file << ", and try again!";
                return false;
            }
        }
        qemu_args.append(" -device virtio-serial,addr=1"
                         " -device virtserialport,chardev=rpmb0,name=rpmb0,nr=1"
                         " -chardev socket,id=rpmb0,path=" + rpmb_sock);
        rpmb_proc = std::make_unique<VmCoProcRpmb>(std::move(rpmb_bin), std::move(rpmb_data_dir), std::move(rpmb_sock));
    }

    std::string vtpm_bin = cfg_.GetValue(kGroupVtpm, kVtpmBinPath);
    std::string vtpm_data_dir = cfg_.GetValue(kGroupVtpm, kVtpmDataDir);
    std::unique_ptr<VmCoProcVtpm> vtpm_proc;
    if (!vtpm_bin.empty() && !vtpm_data_dir.empty()) {
        qemu_args.append(" -chardev socket,id=chrtpm,path=" +
                         vtpm_data_dir + "/" + kVtpmSock +
                         " -tpmdev emulator,id=tpm0,chardev=chrtpm -device tpm-crb,tpmdev=tpm0");
        vtpm_proc = std::make_unique<VmCoProcVtpm>(std::move(vtpm_bin), std::move(vtpm_data_dir));
    }

    std::string firm_type = cfg_.GetValue(kGroupFirm, kFirmType);
    if (firm_type.empty())
        return false;
    if (firm_type.compare(kFirmUnified) == 0) {
        qemu_args.append(" -drive if=pflash,format=raw,file=" + cfg_.GetValue(kGroupFirm, kFirmPath));
    } else if (firm_type.compare(kFirmSplited) == 0) {
        qemu_args.append(" -drive if=pflash,format=raw,readonly,file=" + cfg_.GetValue(kGroupFirm, kFirmCode));
        qemu_args.append(" -drive if=pflash,format=raw,file=" + cfg_.GetValue(kGroupFirm, kFirmVars));
    } else {
        LOG(error) << "Invalid virtual firmware";
        return false;
    }

    qemu_args.append(
        " -device virtio-scsi-pci,id=scsi0,addr=0x8"
        " -drive if=none,format=qcow2,id=scsidisk1,file=" + cfg_.GetValue(kGroupDisk, kDiskPath) +
        " -device scsi-hd,drive=scsidisk1,bus=scsi0.0");

    qemu_args.append(" -name civ_flashing"
        " -M q35"
        " -m 2048M"
        " -smp 1"
        " -enable-kvm"
        " -k en-us"
        " -device qemu-xhci,id=xhci,addr=0x5"
        " -no-reboot"
        " -nographic -display none -serial mon:stdio"
        " -boot menu=on,splash-time=5000,strict=on "
        " -drive id=udisk1,format=raw,if=none,file=" + virtual_disk_ +
        " -device usb-storage,drive=udisk1,bus=xhci.0"
        " -nodefaults");

    if (rpmb_proc)
        rpmb_proc->Run();
    if (vtpm_proc)
        vtpm_proc->Run();

    LOG(info) << qemu_args;
    if (boost::process::system(qemu_args)) {
        LOG(warning) << "Failed to : " << qemu_args;
    }

    LOG(info) << "Flash done!";

    return true;
}

bool VmFlasher::FlashGuest(std::string path) {
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

    if (!cfg_.ReadConfigFile(p.string())) {
        LOG(error) << "Failed to read config file";
        return false;
    }

    std::string emul_type = cfg_.GetValue(kGroupEmul, kEmulType);
    if ((emul_type.compare(kEmulTypeQemu) == 0) || emul_type.empty()) {
        return FlashWithQemu();
    }
    return false;
}

}  // namespace vm_manager
