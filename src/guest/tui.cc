/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <vector>

#include <boost/process.hpp>

#include "guest/tui.h"
#include "utils/utils.h"

#define LAYOUT_MIN_WIDTH 60
#define LAYOUT_MAX_WIDTH 120

namespace vm_manager {

void CivTui::InitName(const std::string& name) {
    name_ = ftxui::Renderer([name] {
        return ftxui::hbox(ftxui::hbox(ftxui::text("name") | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 18),
            ftxui::text(": "), ftxui::text(name)));
    });
}

void CivTui::InitCompDisk(void) {
    cdisk_ = ftxui::Renderer(ftxui::Container::Vertical({disk_size_.Get(), disk_path_.Get()}), [&] {
        return window(
            ftxui::text(std::string("disk") + ":"),
            vbox(disk_size_.Get()->Render(), disk_path_.Get()->Render()));
    });
}

void CivTui::InitCompFirm(void) {
    firmware_type_ = { vm_manager::kFirmUnified, vm_manager::kFirmSplited };

    cfirm_inner_ = ftxui::Container::Vertical({
        ftxui::Toggle(&firmware_type_, &firmware_type_selected_),
        ftxui::Container::Tab({
                firm_unified_.Get(),
                ftxui::Container::Vertical({ firm_splited_code_.Get(), firm_splited_data_.Get() }),
            },
            &firmware_type_selected_
        )
    });
    cfirm_ = ftxui::Renderer(cfirm_inner_, [&] {
        return ftxui::window(ftxui::text("firmware: "), cfirm_inner_->Render());
    });
}

void CivTui::InitCompVgpu(void) {
    vgpu_type_ = {
        vm_manager::kVgpuNone,
        vm_manager::kVgpuVirtio,
        vm_manager::kVgpuRamfb,
        vm_manager::kVgpuGvtG,
        vm_manager::kVgpuGvtD
    };
    cvgpu_type_ = ftxui::Dropdown(&vgpu_type_, &vgpu_selected_);

    gvtg_ver_ = { vm_manager::kGvtgV51, vm_manager::kGvtgV52, vm_manager::kGvtgV54, vm_manager::kGvtgV58 };

    cgvtg_sub_ = ftxui::Container::Vertical({ftxui::Dropdown(&gvtg_ver_, &gvtg_ver_selected_), gvtg_uuid_.Get()});

    cvgpu_side_ = ftxui::Container::Tab({
        ftxui::Renderer(ftxui::emptyElement),  // NONE
        ftxui::Renderer(ftxui::emptyElement),  // VIRTIO
        ftxui::Renderer(ftxui::emptyElement),  // RAMFB
        cgvtg_sub_,                            // GVT-g
        ftxui::Renderer(ftxui::emptyElement),  // GVT-d
    }, &vgpu_selected_);

    cvgpu_ = ftxui::Renderer(ftxui::Container::Horizontal({ cvgpu_type_, cvgpu_side_ }), [&]{
        return window(
            ftxui::text("graphics: "),
            hbox(cvgpu_type_->Render(), cvgpu_side_->Render()));
    });
}

void CivTui::InitCompVtpm(void) {
    cswtpm_ = ftxui::Renderer(ftxui::Container::Vertical({swtpm_bin_.Get(), swtpm_data_.Get()}), [&] {
        return window(
            ftxui::text(std::string("swtpm") + ":"),
            ftxui::vbox(swtpm_bin_.Get()->Render(), swtpm_data_.Get()->Render()));
    });
}

void CivTui::InitCompRpmb(void) {
    crpmb_ = ftxui::Renderer(ftxui::Container::Vertical({rpmb_bin_.Get(), rpmb_data_.Get()}), [&] {
        return window(
            ftxui::text(std::string("rpmb") + ":"),
            ftxui::vbox(rpmb_bin_.Get()->Render(), rpmb_data_.Get()->Render()));
    });
}

void CivTui::InitCompPciPt(void) {
    boost::process::ipstream pipe_stream;
    std::error_code ec;
    boost::process::child c("lspci -D", boost::process::std_out > pipe_stream);
    std::string line;
    while (pipe_stream && std::getline(pipe_stream, line)) {
        if (!line.empty()) {
            CheckBoxState item = { line, false };
            pci_dev_.push_back(item);
        }
        line.erase();
    }

    c.wait(ec);

    PtPciClickButton = [&]() {
        pt_pci_disp_.clear();
        for (std::vector<CheckBoxState>::iterator it = pci_dev_.begin(); it != pci_dev_.end(); ++it) {
            if (it->checked) {
                pt_pci_disp_.push_back(it->name);
            }
        }
        pt_pci_tab_id_ = !pt_pci_tab_id_;
        if (pt_pci_tab_id_ == 1) {
            pt_btn_lable_ = "Done";
            cpt_inner_right_->TakeFocus();
        } else {
            pt_btn_lable_ = "Edit";
        }
    };

    pt_btn_lable_ = "Edit";
    cpt_inner_left_btn_ = ftxui::Button(&pt_btn_lable_, PtPciClickButton);
    cpt_inner_right_edit_ = ftxui::Container::Vertical({});
    for (std::vector<CheckBoxState>::iterator t = pci_dev_.begin(); t != pci_dev_.end(); ++t) {
        cpt_inner_right_edit_->Add(ftxui::Checkbox(t->name, &t->checked));
    }
    cpt_inner_right_disp_ = ftxui::Menu(&pt_pci_disp_, &pt_pci_menu_selected_);

    cpt_inner_right_ = ftxui::Container::Tab({cpt_inner_right_disp_, cpt_inner_right_edit_}, &pt_pci_tab_id_);
    cpt_inner_ = ftxui::Renderer(ftxui::Container::Horizontal({cpt_inner_left_btn_, cpt_inner_right_}), [&]{
        return ftxui::hbox(
            ftxui::vbox(
                cpt_inner_left_btn_->Render() | ftxui::center,
                ftxui::text(std::to_string(pt_pci_disp_.size()) + " Slected") | ftxui::center),
            cpt_inner_right_->Render() | ftxui::frame | ftxui::vscroll_indicator);
    });
    cpt_ = ftxui::Renderer(cpt_inner_, [&]{
        return ftxui::window(ftxui::text("PCI Devices to Passthrough"), cpt_inner_->Render())
             | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 6);
    });
}

void CivTui::InitializeForm(void) {
    InitName(filename_);
    InitCompDisk();
    InitCompFirm();
    InitCompVgpu();
    InitCompVtpm();
    InitCompRpmb();
    InitCompPciPt();

    form_ = ftxui::Container::Vertical({
        name_,
        flashfiles_.Get(),
        emulator_.Get(),
        memory_.Get(),
        vcpu_.Get(),
        cdisk_,
        cfirm_,
        cvgpu_,
        cswtpm_,
        crpmb_,
        cpt_,
        adb_port_.Get(),
        fastboot_port_.Get(),
        batt_med_.Get(),
        therm_med_.Get(),
        aaf_.Get(),
        tkeep_.Get(),
        pmctl_.Get(),
        extra_.Get(),
    });

    form_render_ = ftxui::Renderer(form_, [&]{
        ftxui::Element eform_ = ftxui::vbox(form_->Render()) | ftxui::frame | ftxui::vscroll_indicator;
        return eform_;
    });
}

void CivTui::InitializeButtons(void) {
    SaveOn = [&]() {
        std::string configPath = GetConfigPath();
        std::string filePath = configPath + "/" + filename_ +".ini";
        boost::system::error_code ec;
        if (!boost::filesystem::exists(filePath, ec)) {
            boost::filesystem::ofstream file(filePath);
            SetConfToPtree();
            bool writeConfigFile = civ_config_.WriteConfigFile(filePath);
            if (writeConfigFile) {
                screen_.ExitLoopClosure()();
                LOG(info) << "Config saved!" << std::endl;
            } else {
                LOG(warning) << "Write config file failue!" << std::endl;
            }
            file.close();
        } else {
            status_bar_ = "File alreay exist!";
        }
        screen_.Clear();
    };

    save_ = ftxui::Button("SAVE", [&]{SaveOn();});
    exit_ = ftxui::Button("EXIT", screen_.ExitLoopClosure());

    buttons_ = ftxui::Container::Horizontal({
        save_, exit_
    });
}

void CivTui::InitializeUi(std::string name) {
    filename_ = name;
    InitializeForm();
    InitializeButtons();

    layout_ = ftxui::Container::Vertical({
        form_render_,
        buttons_,
    });

    auto layout_render = ftxui::Renderer(layout_, [&] {
        return ftxui::vbox({
            ftxui::text("Celadon in VM Configuration"),
            ftxui::vbox({form_render_->Render() | ftxui::vscroll_indicator }) | ftxui::border,
            ftxui::text(status_bar_),
            buttons_->Render() | ftxui::center,
        }) | ftxui::border | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, LAYOUT_MIN_WIDTH)
           | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 80) | ftxui::center;
    });

    screen_.Loop(layout_render);
}

void CivTui::SetConfToPtree() {
    civ_config_.SetValue(kGroupGlob, kGlobName, filename_);
    civ_config_.SetValue(kGroupGlob, kGlobFlashfiles, flashfiles_.GetContent());

    civ_config_.SetValue(kGroupEmul, kEmulPath, emulator_.GetContent());

    civ_config_.SetValue(kGroupMem, kMemSize, memory_.GetContent());

    civ_config_.SetValue(kGroupVcpu, kVcpuNum, vcpu_.GetContent());

    civ_config_.SetValue(kGroupDisk, kDiskSize, disk_size_.GetContent());
    civ_config_.SetValue(kGroupDisk, kDiskPath, disk_path_.GetContent());

    civ_config_.SetValue(kGroupFirm, kFirmType, firmware_type_.at(firmware_type_selected_));
    if (firmware_type_selected_ == 0) {
        civ_config_.SetValue(kGroupFirm, kFirmPath, firm_unified_.GetContent());
    } else if (firmware_type_selected_ == 1) {
        civ_config_.SetValue(kGroupFirm, kFirmCode, firm_splited_code_.GetContent());
        civ_config_.SetValue(kGroupFirm, kFirmVars, firm_splited_data_.GetContent());
    }

    civ_config_.SetValue(kGroupVgpu, kVgpuType, vgpu_type_.at(vgpu_selected_));
    if (vgpu_selected_ == 3) {
        civ_config_.SetValue(kGroupVgpu, kVgpuGvtgVer, gvtg_ver_.at(gvtg_ver_selected_));
        if (!gvtg_uuid_.GetContent().empty()) {
            civ_config_.SetValue(kGroupVgpu, kVgpuUuid, gvtg_uuid_.GetContent());
        }
    }

    civ_config_.SetValue(kGroupVtpm, kVtpmBinPath, swtpm_bin_.GetContent());
    civ_config_.SetValue(kGroupVtpm, kVtpmDataDir, swtpm_data_.GetContent());

    civ_config_.SetValue(kGroupRpmb, kVtpmBinPath, rpmb_bin_.GetContent());
    civ_config_.SetValue(kGroupRpmb, kVtpmDataDir, rpmb_data_.GetContent());

    std::string passthrough;
    for (std::vector<std::string>::iterator iter = pt_pci_disp_.begin(); iter != pt_pci_disp_.end(); iter++) {
        std::string selectItem = *iter;
        if (selectItem.length() == 0)
            continue;
        size_t pos = selectItem.find(" ");
        if (pos == std::string::npos)
            continue;
        passthrough += selectItem.substr(0, pos);
        if (iter != (pt_pci_disp_.end() - 1)) {
            passthrough += ',';
        }
    }

    civ_config_.SetValue(kGroupPciPt, kPciPtDev, passthrough);

    civ_config_.SetValue(kGroupNet, kNetAdbPort, adb_port_.GetContent());

    civ_config_.SetValue(kGroupNet, kNetFastbootPort, fastboot_port_.GetContent());

    civ_config_.SetValue(kGroupMed, kMedBattery, batt_med_.GetContent());

    civ_config_.SetValue(kGroupMed, kMedThermal, therm_med_.GetContent());

    civ_config_.SetValue(kGroupAaf, kAafPath, aaf_.GetContent());

    civ_config_.SetValue(kGroupService, kServTimeKeep, tkeep_.GetContent());

    civ_config_.SetValue(kGroupService, kServPmCtrl, pmctl_.GetContent());

    civ_config_.SetValue(kGroupExtra, kExtraCmd, extra_.GetContent());
}

}  // namespace vm_manager

