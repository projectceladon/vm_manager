/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef SRC_GUEST_TUI_H_
#define SRC_GUEST_TUI_H_

int create_tui(void);

#include <memory>
#include <string>
#include <vector>
#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include "ftxui/component/captured_mouse.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/util/ref.hpp"
#include "ftxui/screen/terminal.hpp"

#include "config_parser.h"
#include "utils/log.h"

typedef struct {
    std::string name;
    bool checked;
} CheckBoxState;

class InputField {
 public:
    InputField(const char *name, const char *hint)
        :name_(name) {
        input_ = ftxui::Input(&content_, std::string(hint));
        wrap_ = ftxui::Renderer(input_, RenderInput);
     }

     ftxui::Component Get() {
         return wrap_;
     }

     std::string GetContent() {
        return content_;
     }

 private:
    const char *name_;
    ftxui::Component wrap_;
    ftxui::Component input_;
    std::string content_;
    std::function<ftxui::Element()> RenderInput = [this]() {
        return ftxui::hbox(ftxui::hbox(ftxui::text(std::string(name_)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 18),
                                       ftxui::text(": ")),
                           input_->Render());
    };
};

namespace vm_manager {

class CivTui final {
 public:
    void InitializeUi(std::string filename);

 private:
    std::string filename_;
    ftxui::ScreenInteractive screen_ = ftxui::ScreenInteractive::Fullscreen();
    ftxui::Component layout_;
    ftxui::Component form_, form_render_, form_main_;
    ftxui::Component buttons_;

    /* Field: name */
    ftxui::Component name_;
    void InitName(const std::string& name);

    /* Field: flashfiles */
    InputField flashfiles_ = InputField("flashfiles", "flashfiles path");

    /* Field: emulator */
    InputField emulator_ = InputField("emulator", "emulator path");

    /* Field: emulator */
    InputField memory_ = InputField("memory", "memory size");

    /* Field: vcpu */
    InputField vcpu_ = InputField("vcpu", "vcpu number(1~1024)");

    /* Field: disk size/path */
    ftxui::Component cdisk_;
    InputField disk_size_ = InputField("disk size", "virtual disk size");
    InputField disk_path_ = InputField("disk path", "virtual disk path");
    void InitCompDisk(void);

    /* Field: firmware */
    ftxui::Component cfirm_;
    ftxui::Component cfirm_inner_;
    int firmware_type_selected_ = 0;
    std::vector<std::string> firmware_type_;
    InputField firm_unified_ = InputField("bin path", "Unified firmware binary path");
    InputField firm_splited_code_ = InputField("code path", "Splited firmware binary code path");
    InputField firm_splited_data_ = InputField("data path", "Splited firmware binary data path");
    void InitCompFirm(void);

    /* Field: vgpu */
    std::vector<std::string> vgpu_type_;
    int vgpu_selected_ = 1;
    ftxui::Component cvgpu_;
    ftxui::Component cvgpu_type_;
    ftxui::Component cvgpu_side_;
    void InitCompVgpu(void);

    int gvtg_ver_selected_ = 1;
    std::vector<std::string> gvtg_ver_;
    InputField gvtg_uuid_ = InputField("uuid", "UUID for a GVTg-VGPU");
    ftxui::Component cgvtg_sub_;

    /* Field: SWTPM */
    ftxui::Component cswtpm_;
    InputField swtpm_bin_ = InputField("bin path", "binary path");
    InputField swtpm_data_ = InputField("data dir", "data folder");
    void InitCompVtpm(void);

    /* Field: RPMB */
    ftxui::Component crpmb_;
    InputField rpmb_bin_ = InputField("bin path", "binary path");
    InputField rpmb_data_ = InputField("data dir", "data folder");
    void InitCompRpmb(void);

    /* Field: PCI Passthrough devices */
    std::vector<CheckBoxState> pci_dev_;
    std::vector<std::string> pt_pci_disp_;
    std::string pt_btn_lable_;
    int pt_pci_tab_id_ = 0;
    int pt_pci_menu_selected_ = 0;
    ftxui::Component cpt_;
    ftxui::Component cpt_inner_, cpt_inner_left_btn_, cpt_inner_right_, cpt_inner_right_edit_, cpt_inner_right_disp_;
    void InitCompPciPt(void);
    std::function<void(void)> PtPciClickButton;

    /* Fields: adb/fastboot port */
    InputField adb_port_ = InputField("adb port", "ADB port(TCP-Localhost), e.g.:5555");
    InputField fastboot_port_ = InputField("fastboot port", "Fastboot port(TCP-Localhost), e.g.:5554");

    /* Fields: Battery/Thermal mediation/aaf/Time Keep/PM control/Extra */
    InputField batt_med_ = InputField("Battery Mediation", "Battery Mediation host side executable binary");
    InputField therm_med_ = InputField("Thermal Mediation", "Thermal Mediation host side executable binary");
    InputField aaf_ = InputField("AAF", "AAF configuration folder path");
    InputField tkeep_ = InputField("Guest Time Keep", "Time Keep host side executable binary");
    InputField pmctl_ = InputField("Guest PM Control", "PM Control host side executable binary");
    InputField extra_ = InputField("Extra", "Extra command that emulator can acccept");

    /* Status Bar */
    std::string status_bar_;

    /* Buttons */
    vm_manager::CivConfig civ_config_;
    ftxui::Component save_;
    ftxui::Component exit_;
    std::function<void(void)> SaveOn;

    void InitializeForm(void);
    void InitializeButtons(void);
    void SetConfToPtree();
};

}  // namespace vm_manager

#endif  // SRC_GUEST_TUI_H_

