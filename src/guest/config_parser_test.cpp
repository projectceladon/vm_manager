
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
namespace pt = boost::property_tree;

#include <assert.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <ctime>
using namespace std;

//#include "guest.h"
#include "config_parser.h"

po::options_description set_options(void)
{
    po::options_description opts;
    opts.add_options()
        ("global.name", po::value<string>())
        ("global.flashfiles", po::value<string>())
        ("vcpu.num", po::value<string>())
        ("vgpu.num", po::value<string>())
        ;

    return opts;
}


vector<string> parse_file(const char *file, po::options_description &opts, po::variables_map &vm)
{
    const bool ALLOW_UNREGISTERED = true;
    //try {
    po::parsed_options parsed = parse_config_file(file, opts, ALLOW_UNREGISTERED);
    store(parsed, vm);
    vector<string> unregistered = po::collect_unrecognized(parsed.options, po::exclude_positional);
    notify(vm);
    return unregistered;
    //} catch (exception& e) {
    //    cout << e.what() << endl;
    //}

}
#if 0
static map<string, vector<string>> keys_global = {
    { "name",          {}},
    { "flashfiles",    {}},
    { "adb_port",      {}},
    { "fastboot_port", {}},
    { "vsock_cid",     {}},
};

static map<string, vector<string>> keys_emulator = {
    {"type", {
        "QEMU"
    }},
    {"path", {}},
};

static map<string, vector<string>> keys_memory = {
    {"size", {}},
};

static map<string, vector<string>> keys_vcpu = {
    {"num", {}},
};

static map<string, vector<string>> keys_firmware = {
    {"type", {
        FIRM_OPTS_UNIFIED_STR,
        FIRM_OPTS_SPLITED_STR
    }},
    {"path", {}},
};

static map<string, vector<string>> keys_disk = {
    {"size", {}},
    {"path", {}},
};

static map<string, vector<string>> keys_graphics = {
    {"type", {
        VGPU_OPTS_NONE_STR,
        VGPU_OPTS_VIRTIO_STR,
        VGPU_OPTS_RAMFB_STR,
        VGPU_OPTS_GVTG_STR,
        VGPU_OPTS_GVTD_STR
    }},
    {"gvtg_version", {
        GVTG_OPTS_V5_1_STR,
        GVTG_OPTS_V5_2_STR,
        GVTG_OPTS_V5_4_STR,
        GVTG_OPTS_V5_8_STR
    }},
};

static map<string, vector<string>> keys_vtpm = {
    {"bin_path", {}},
    {"data_dir", {}},
};

static map<string, vector<string>> keys_rpmb = {
    {"bin_path", {}},
    {"data_dir", {}},
};

static map<string, vector<string>> keys_aaf = {
    {"path", {}},
    {"support_suspend", {
        SUSPEND_ENABLE_STR,
        SUSPEND_DISABLE_STR
    }},
};

static map<string, vector<string>> keys_passthrough = {
    {"passthrough_pci", {}},
};

static map<string, vector<string>> keys_mediation = {
    {"battery_med", {}},
    {"thermal_med", {}},
};

static map<string, vector<string>> keys_guest_control = {
    {"time_keep", {}},
    {"pm_control", {}},
};

static map<string, vector<string>> keys_extra = {
    {"cmd", {}},
};

static map<string, map<string, vector<string>>> civ_mopts = {
    { "global",        keys_global, },
    { "emulator",      keys_emulator },
    { "memory",        keys_memory },
    { "vcpu",          keys_vcpu },
    { "firmware",      keys_firmware },
    { "disk",          keys_disk },
    { "graphics",      keys_graphics },
    { "vtpm",          keys_vtpm },
    { "rpmb",          keys_rpmb },
    { "aaf",           keys_aaf },
    { "passthrough",   keys_passthrough },
    { "mediation",     keys_mediation },
    { "guest_control", keys_guest_control },
    { "extra",         keys_extra },
};
#endif

int parse_file_ptree(const char *file)
{
#if 0
std::map<std::string, std::vector<std::string>> civ_opts = {
    {"global.name",                 {}},
    {"global.flashfiles",           {}},
    {"global.adb_port",             {}},
    {"global.fastboot_port",        {}},
    {"global.vsock_cid",            {}},

    {"emulator.type",               { "QEMU" }},
    {"emulator.path",               {}},

    {"memory.size",                 {}},

    {"vcpu.num",                    {}},

    {"firmware.type",               { FIRM_OPTS_UNIFIED_STR, FIRM_OPTS_SPLITED_STR }},
    {"firmware.path",               {}},

    {"disk.size",                   {}},
    {"disk.path",                   {}},

    {"graphics.type",               {VGPU_OPTS_NONE_STR, VGPU_OPTS_VIRTIO_STR, VGPU_OPTS_RAMFB_STR, VGPU_OPTS_GVTG_STR, VGPU_OPTS_GVTD_STR}},
    {"graphics.gvtg_version",       {GVTG_OPTS_V5_1_STR, GVTG_OPTS_V5_2_STR, GVTG_OPTS_V5_4_STR, GVTG_OPTS_V5_8_STR}},

    {"vtpm.bin_path",               {}},
    {"vtpm.data_dir",               {}},

    {"rpmb.bin_path",               {}},
    {"rpmb.data_dir",               {}},

    {"aaf.path",                    {}},
    {"aaf.support_suspend",         {SUSPEND_ENABLE_STR, SUSPEND_DISABLE_STR}},

    {"passthrough.passthrough_pci", {}},

    {"mediation.battery_med",       {}},
    {"mediation.thermal_med",       {}},

    {"guest_control",               {}},

    {"extra",                       {}},
};
#endif


    pt::ptree root;
    string fs(file);
    read_ini(fs, root);
    std::srand(std::time(nullptr));
    for (auto& section : root) {
        cout << "[" << section.first << "]" << endl;
        //if (section.first == "global") {
        //auto grp = find_if(civ_opts.begin(), civ_opts.end(),
        //            [&section](const pair<string, vector<string>>& element) { return element.first == section.first;});
        auto it = civ_mopts.find(section.first);
        if (it != civ_mopts.end()) {
            cout << "civ_mopts: found[group]: " << section.first << endl;
        }



            for (auto& key : section.second) {
                auto it2 = it->second.find(key.first);
                if (it2 != it->second.end()) {
                    cout << "civ_mopts: found[key]: " << key.first << endl;
                }
                cout << key.first << "=" << key.second.get_value<std::string>() << endl;
                if (key.first == "name") {
                    std::string s( "TEST" + std::to_string(std::rand()%1000));
                    key.second.put_value(s);
                }
            }
        //}
    }

    string na = root.get<string>("global.name");
    cout << "stringna=" << na << endl;

    try {
      int nna = root.get<int>("global.na");
      cout << "intna=" << nna << endl;
    } catch (exception& e) {
        cout << "Exception: Cannot get global.na" << endl;
    }

    root.put("global.name", "xxxyyy" + std::to_string(std::rand()%1000));

    int vnum = root.get<int>("vcpu.num");
    cout << "stringvnum=" << vnum << endl;
    root.put("vcpu.num", std::rand()%100);
    root.put("vcpu.threads", std::rand()%100);

    root.put("vxpu.x", std::rand()%100);
    root.put("vxpu.y", std::rand()%100);

    try {
        string s = root.get<string>("global.dds");
        cout << "value=" << s << endl;
    } catch (exception& e) {
        cout << "Exception: Cannot get global.dds" << endl;
    }

    try {
        string s = root.get<string>("global");
        cout << "global=" << s << endl;
    } catch (exception& e) {
        cout << "Exception: Cannot get global" << endl;
    }

    try {
        string s = root.get<string>("global.name");
        cout << "global.name=" << s << endl;
    } catch (exception& e) {
        cout << "Exception: Cannot get global" << endl;
    }

    string to_find = "global.sss";
    auto it = root.find(to_find);
    if (it == root.not_found()) {
        cout << "Can not find the keys:" << to_find << endl;
    } else {
        cout << "Found the keys:" << to_find << endl;
    }

    to_find = "global.sssx";
    it = root.find(to_find);
    if (it == root.not_found()) {
        cout << "Can not find the keys:" << to_find << endl;
    } else {
        cout << "Found the keys:" << to_find <<endl;
    }

    to_find = "global";
    it = root.find(to_find);
    if (it == root.not_found()) {
        cout << "Can not find the keys:" << to_find << endl;
    } else {
        cout << "Found the keys:" << to_find <<endl;
    }

    cout << "Write to File" << endl;
    write_ini(file, root);
}

void dump_results(po::variables_map &vm, vector<string> unregistered)
{
    //cout << vm["global.name"].as<string>() << endl;
    //cout << vm["global.flashfiles"].as<string>() << endl;
    //cout << vm["vcpu.num"].as<string>() << endl;
    for (auto& it : vm) {
        cout << it.first.c_str() << "=";
        auto& value = it.second.value();
        if (auto v = boost::any_cast<std::string>(&value))
            cout << *v << endl;
    }
    for (auto & e : unregistered) {
        cout << e << endl;
        cout << endl;
    }
}

int parse_ini_file(const char *file)
{
    auto opts = set_options();
    po::variables_map vars;
    try {
    //auto unregistered = parse_file("/ssd_disk4/projects/celadon_pr/vm_manager/sample/guest01.ini", opts, vars);
    //cout << "Parsed done!" << endl;
    //dump_results(vars, unregistered);

    parse_file_ptree(file);

    return 0;
    } catch (exception& e) {
        cout << "Exception: " << e.what() << endl;
        return -1;
    }

}