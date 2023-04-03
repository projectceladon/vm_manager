/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <fstream>

#include <boost/filesystem.hpp>

#include "guest/aaf.h"
#include "utils/log.h"

namespace vm_manager {

const char *kAafKeyGpuType = "gpu-type";
const char *kAafKeySuspend = "suspend";
const char *kAafKeyAudioType = "audio-type";

void Aaf::Set(std::string k, std::string v) {
    auto find = data_.find(k);
    if (find != data_.end()) {
        find->second = v;
    }
}

void Aaf::Flush(void) {
    boost::system::error_code ec;
    if (path_.empty()) {
        LOG(warning) << "Aaf path not set!";
        return;
    }

    if (!boost::filesystem::exists(path_, ec)) {
        LOG(warning) << "Aaf path not exists!";
        return;
    }

    if (!boost::filesystem::is_directory(path_, ec)) {
        LOG(warning) << "Aaf path is not directory!";
        return;
    }

    std::string target_file(path_ + "/" + "mixins.spec");
    std::ofstream out;

    if (boost::filesystem::exists(target_file, ec)) {
        std::string hidden_file(path_ + "/" + ".mixins.spec~");
        boost::filesystem::rename(target_file, hidden_file, ec);
        std::ifstream in(hidden_file, std::ios::in);
        out.open(target_file);
        std::string tmp;
        LOG(info) << target_file;
        while (!in.eof()) {
            getline(in, tmp);
            if (tmp.empty())
                continue;
            std::string key = tmp.substr(0, tmp.find(":"));
            if (key.length() == 0)
                continue;
            auto find = data_.find(key);
            if ((find != data_.end()) && !find->second.empty()) {
                out << key << ":" << find->second << "\n";
                find->second.clear();
            } else {
                out << tmp << "\n";
            }
        }
        in.close();
        boost::filesystem::remove(hidden_file, ec);
    }

    if (!out.is_open()) {
        out.open(target_file);
    }

    for (auto it = data_.begin(); it != data_.end(); it++) {
        if (!it->second.empty()) {
            out << it->first << ":" << it->second << "\n";
            it->second.clear();
        }
    }

    out.close();
}

Aaf::Aaf(const char *path) : path_(path) {
    data_.insert(std::pair<std::string, std::string>(kAafKeyGpuType, std::string()));
    data_.insert(std::pair<std::string, std::string>(kAafKeySuspend, std::string()));
    data_.insert(std::pair<std::string, std::string>(kAafKeyAudioType, std::string()));
}

}  //  namespace vm_manager
