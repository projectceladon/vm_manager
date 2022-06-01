/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <vector>

#include "guest/tui.h"
#include "guest/config_parser.h"

#include <boost/process.hpp>

#define LAYOUT_MIN_WIDTH 60
#define LAYOUT_MAX_WIDTH 120

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
    InitCompDisk();
    InitCompFirm();
    InitCompVgpu();
    InitCompVtpm();
    InitCompRpmb();
    InitCompPciPt();

    form_ = ftxui::Container::Vertical({
        name_.Get(),
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
        screen_.ExitLoopClosure();
        status_bar_ += "called SaveOn!";
        screen_.Clear();
    };

    save_ = ftxui::Button("SAVE", [&]{SaveOn();});
    exit_ = ftxui::Button("EXIT", screen_.ExitLoopClosure());

    buttons_ = ftxui::Container::Horizontal({
        save_, exit_
    });
}

void CivTui::InitializeUi(void) {
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

