/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <assert.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>
#include <string_view>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "guest/config_parser.h"
#include "utils/log.h"

namespace vm_manager {

using std::map;
using std::vector;
using std::string_view;
using boost::property_tree::ptree;

map<string_view, vector<string_view>> kConfigMap = {
    { kGroupGlob,    { kGlobName, kGlobFlashfiles, kGlobCid, kGlobWaitReady } },
    { kGroupEmul,    { kEmulType, kEmulPath } },
    { kGroupMem,     { kMemSize } },
    { kGroupVcpu,    { kVcpuNum } },
    { kGroupFirm,    { kFirmType, kFirmPath, kFirmCode, kFirmVars } },
    { kGroupDisk,    { kDiskSize, kDiskPath } },
    { kGroupVgpu,    { kVgpuType, kVgpuGvtgVer, kVgpuUuid, kVgpuMonId, kVgpuOutputs } },
    { kGroupDisplay, { kDispOptions } },
    { kGroupNet,     { kNetModel, kNetAdbPort, kNetFastbootPort } },
    { kGroupVtpm,    { kVtpmBinPath, kVtpmDataDir } },
    { kGroupRpmb,    { kRpmbBinPath, kRpmbDataDir } },
    { kGroupAaf,     { kAafPath, kAafSuspend, kAafAudioType }},
    { kGroupPciPt,   { kPciPtDev }},
    { kGroupbluetooth, { kHciDown }},
    { kGroupAudio,   { kDisableEmul } },
    { kGroupMed,     { kMedBattery, kMedThermal, kMedCamera } },
    { kGroupService, { kServTimeKeep, kServPmCtrl, kServVinput } },
    { kGroupExtra,   { kExtraCmd, kExtraService, kExtraPwrCtrlMultiOS } }
};

bool CivConfig::SanitizeOpts(void) {
    for (auto& section : cfg_data_) {
        auto group = kConfigMap.find(section.first);
        if (group != kConfigMap.end()) {
            for (auto& subsec : section.second) {
                auto key = std::find(group->second.begin(), group->second.end(), subsec.first);
                if (key == group->second.end()) {
                    LOG(error) << "Invalid key: " << section.first << "." << subsec.first << "\n";
                    return false;
                }
            }
        } else {
            LOG(error) << "Invalid group: " << section.first << "\n";
            return false;
        }
    }
    return true;
}

bool CivConfig::ReadConfigFile(std::string path) {
    boost::system::error_code ec;
    if (!boost::filesystem::exists(path, ec)) {
        LOG(error) << "File not exists: " << path << std::endl;
        return false;
    }

    if (!boost::filesystem::is_regular_file(path, ec)) {
        LOG(error) << "File is not a regular file: " << path << std::endl;
        return false;
    }

    cfg_data_.clear();
    read_ini(path, cfg_data_);

    if (!SanitizeOpts()) {
        cfg_data_.clear();
        return false;
    }
    return true;
}

bool CivConfig::WriteConfigFile(std::string path) {
    boost::system::error_code ec;
    if (!boost::filesystem::exists(path, ec)) {
        LOG(error) << "File not exists: " << path << std::endl;
        return false;
    }

    if (!boost::filesystem::is_regular_file(path, ec)) {
        LOG(error) << "File is not a regular file: " << path << std::endl;
        return false;
    }

    if (cfg_data_.empty()) {
        LOG(warning) << "No data to write, skip!" << std::endl;
        return true;
    }

    if (!SanitizeOpts()) {
        return false;
    }

    write_ini(path, cfg_data_);
    return true;
}

std::string CivConfig::GetValue(std::string group, std::string key) {
    try {
        std::string val = cfg_data_.get<std::string>(group + "." + key);
        boost::trim(val);
        return val;
    } catch(const std::exception& e) {
        LOG(warning) << e.what();
        return std::string();
    }
}

bool CivConfig::SetValue(const std::string group, const std::string key, const std::string value) {
    if (!value.empty()) {
        try {
            cfg_data_.put<std::string>(group + "." + key, value);
        } catch(const std::exception& e) {
            LOG(warning) << e.what();
            return false;
        }
    }
    return true;
}

}  // namespace vm_manager

