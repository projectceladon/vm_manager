#!/bin/bash

# Copyright (c) 2020 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

set -eE

#------------------------------------------------------      Global variable    ----------------------------------------------------------
WORK_DIR=$PWD
SCRIPTS_DIR=$WORK_DIR/scripts

EMULATOR_PATH=$(which qemu-system-x86_64)
GUEST_MEM="-m 2G"
GUEST_CPU_NUM="-smp 1"
GUEST_DISK="-drive file=$WORK_DIR/android.qcow2,if=none,id=disk1"
GUEST_FIRMWARE="-drive file=$WORK_DIR/OVMF.fd,format=raw,if=pflash"
GUEST_DISP_TYPE="-display gtk,gl=on"
GUEST_VGA_DEV="-device virtio-gpu-pci"
GUEST_RPMB_DEV=
GUEST_RPMB_DEV_PID=
GUEST_RPMB_DEV_SOCK=
GUEST_THERMAL_DAEMON_PID=
GUEST_BATTERY_DAEMON_PID=
GUEST_IMAGE=$WORK_DIR/android.qcow2
GUEST_VSOCK="-device vhost-vsock-pci,id=vhost-vsock-pci0,guest-cid=3"
GUEST_SHARE_FOLDER=
GUEST_SMBIOS_SERIAL=$(dmidecode -t 2 | grep -i serial | awk '{print $3}')
GUEST_QMP_SOCK=$WORK_DIR/.civ.qmp.sock
GUEST_QMP_UNIX="-qmp unix:$GUEST_QMP_SOCK,server,nowait"
GUEST_NET="-device e1000,netdev=net0 -netdev user,id=net0,hostfwd=tcp::5555-:5555,hostfwd=tcp::5554-:5554"
GUEST_BLK_DEV=
GUEST_AUDIO_DEV="-device intel-hda -device hda-duplex -audiodev id=android_spk,timer-period=5000,server=$XDG_RUNTIME_DIR/pulse/native,driver=pa"
GUEST_EXTRA_QCMD=
GUEST_USB_PT_DEV=
GUEST_AUDIO_PT_DEV=
GUEST_ETH_PT_DEV=
GUEST_WIFI_PT_DEV=
GUEST_PM_CTRL=
GUEST_TIME_KEEP=
GUSET_VTPM="-chardev socket,id=chrtpm,path=$WORK_DIR/vtpm0/swtpm-sock -tpmdev emulator,id=tpm0,chardev=chrtpm -device tpm-crb,tpmdev=tpm0"

GUEST_STATIC_OPTION="\
 -name caas-vm \
 -M q35 \
 -machine kernel_irqchip=off \
 -enable-kvm \
 -k en-us \
 -cpu host \
 -device qemu-xhci,id=xhci \
 -usb \
 -device usb-kbd \
 -chardev socket,id=charserial0,path=$WORK_DIR/kernel-console,server,nowait,logfile=$WORK_DIR/civ_serial.log \
 -serial chardev:charserial0 \
 -device virtio-blk-pci,drive=disk1,bootindex=1 \
 -device intel-iommu,device-iotlb=off,caching-mode=on \
 -smbios type=2,serial=$GUEST_SMBIOS_SERIAL \
 -nodefaults"


#------------------------------------------------------         Functions       ----------------------------------------------------------
function check_nested_vt() {
    local nested=$(cat /sys/module/kvm_intel/parameters/nested)
    if [[ $nested != 1 && $nested != 'Y' ]]; then
        echo "E: Nested VT is not enabled!"
        return -1
    fi
}

function check_kernel_version() {
    local cur_ver=$(uname -r | sed "s/\([0-9.]*\).*/\1/")
    local req_ver="5.0.0"

    if [ "$(printf '%s\n' "$cur_ver" "$req_ver" | sort -V | head -n1)" != "$req_ver" ]; then
        echo "E: Detected Linux version: $cur_ver!"
        echo "E: Please upgrade kernel version newer than $req_ver!"
        return -1
    fi
}

function setup_audio_dev() {
    local AUDIO_SETUP=$SCRIPTS_DIR/setup_audio_host.sh
    $AUDIO_SETUP setMicGain &
}

function kill_daemon_proc() {
    local pid=$1
    local name=$2
    if [[ ! -z "$pid" && $(ps -p $pid -o comm=) == $name ]]; then
        kill $pid
    fi
}

function setup_rpmb_dev() {
    local RPMB_DEV=$SCRIPTS_DIR/rpmb_dev
    local RPMB_DATA=$WORK_DIR/RPMB_DATA
    GUEST_RPMB_DEV_SOCK=$WORK_DIR/rpmb_sock
    local RPMB_INIT_KEY="ea df 64 44 ea 65 5d 1c 87 27 d4 20 71 0d 53 42 dd 73 a3 38 63 e1 d7 94 c3 72 a6 ea e0 64 64 e6"
    # RPMB_DATA is created and initialized with specific key, if this file
    # is deleted by accidently, create a new one without any data.
    if [ ! -f $RPMB_DATA ]; then
        echo "Creating RPMB DATA..."
        $RPMB_DEV --dev $RPMB_DATA --init --key "$RPMB_INIT_KEY" --size 2048
    fi

    # RPMB sock should be removed at cleanup, if there exists RPMB sock,
    # rpmb_dev cannot be launched succefully. Delete any exist RPMB sock,
    # rpmb_dev application creates RPMB sock by itself.
    if [ -S $GUEST_RPMB_DEV_SOCK ]; then
        rm $GUEST_RPMB_DEV_SOCK
    fi

    # rmpb_dev should be launched in background process
    $RPMB_DEV --dev $RPMB_DATA --sock $GUEST_RPMB_DEV_SOCK &
    local rpmb_dev_pid=$!

    local wait_cnt=0
    while [ ! -S $GUEST_RPMB_DEV_SOCK ]
    do
        sleep 1
        (( wait_cnt++ ))
        if [ $wait_cnt -ge 10 ]; then
            echo "E: Failed to setup virtual RPMB device!"
            return -1
        fi
    done

    GUEST_RPMB_DEV="-device virtio-serial -device virtserialport,chardev=rpmb0,name=rpmb0 -chardev socket,id=rpmb0,path=$GUEST_RPMB_DEV_SOCK"
    GUEST_RPMB_DEV_PID=$rpmb_dev_pid
}

function cleanup_rpmb_dev() {
    kill_daemon_proc "$GUEST_RPMB_DEV_PID" "rpmb_dev"
    [[ -z "$GUEST_RPMB_DEV_SOCK" ]] || ( [[ -S $GUEST_RPMB_DEV_SOCK ]] && rm $GUEST_RPMB_DEV_SOCK )
}

function setup_swtpm() {
    #start software Trusted Platform Module
    mkdir -p $WORK_DIR/vtpm0
    swtpm socket --tpmstate dir=$WORK_DIR/vtpm0 --tpm2 --ctrl type=unixio,path=$WORK_DIR/vtpm0/swtpm-sock &
}

function setup_thermal_mediation() {
    local thermal_daemon=$SCRIPTS_DIR/thermsys
    if [ -f $thermal_daemon ]; then
        $thermal_daemon &
        GUEST_THERMAL_DAEMON_PID=$!
    fi
}

function cleanup_thermal_mediation() {
    kill_daemon_proc "$GUEST_THERMAL_DAEMON_PID" "thermsys"
}

function setup_battery_mediation() {
    local battery_daemon=$SCRIPTS_DIR/batsys
    if [ -f $battery_daemon ]; then
        $battery_daemon &
        GUEST_BATTERY_DAEMON_PID=$!
    fi
}

function cleanup_battery_mediation() {
    kill_daemon_proc "$GUEST_BATTERY_DAEMON_PID" "batsys"
}

function create_gvtg_vgpu() {
    local GVTg_DEV_PATH="/sys/bus/pci/devices/0000:00:02.0"
    local GVTg_VGPU_TYPE="i915-GVTg_V5_4"
    local vgpu_sys_dev=$1
    local GVTg_VGPU_UUID="4ec1ff92-81d7-11e9-aed4-5bf6a9a2bb0a"
    [ ! -z $2 ] && GVTg_VGPU_UUID=$2
    if [ ! -d $GVTg_DEV_PATH/$GVTg_VGPU_UUID ]; then
        echo "Creating VGPU..."
        sudo sh -c "echo $GVTg_VGPU_UUID > $GVTg_DEV_PATH/mdev_supported_types/$GVTg_VGPU_TYPE/create"
        if [ $? != 0 ]; then
            echo "E: Failed to create $GVTg_DEV_PATH/$GVTg_VGPU_UUID!"
            return -1
        fi
        echo "GVTg-VGPU: $GVTg_DEV_PATH/$GVTg_VGPU_UUID created."
    fi
    eval $vgpu_sys_dev="$GVTg_DEV_PATH/$GVTg_VGPU_UUID"
}

function set_mem() {
    GUEST_MEM="-m $1"
}

function set_cpu() {
    GUEST_CPU_NUM="-smp $1"
}

function set_disk() {
    GUEST_DISK="-drive file=$1,if=none,id=disk1"
}

function set_firmware() {
    GUEST_FIRMWARE="-drive file=$1,format=raw,if=pflash"
}

function set_vsock() {
    GUEST_VSOCK="-device vhost-vsock-pci,id=vhost-vsock-pci0,guest-cid=$1"
}

function set_share_folder() {
    sudo modprobe 9pnet && \
    sudo modprobe 9pnet_virtio && \
    sudo modprobe 9p && \
    GUEST_SHARE_FOLDER="-fsdev local,security_model=none,id=fsdev0,path=$1 -device virtio-9p-pci,fsdev=fsdev0,mount_tag=hostshare"
}

function set_graphics() {
    if [[ $1 =~ ^GVT-d ]]; then
        if [[ $1 == "GVT-d" ]]; then
            GUEST_VGA_DEV="-device vfio-pci,host=00:02.0,x-igd-gms=2,id=gvtd_dev,bus=pcie.0,addr=0x2,x-igd-opregion=on"
        elif [[ $1 =~ ^GVT-d,romfile=* ]]; then
            local GVTD_ROM=${1#*=}
            [ -z $GVTD_ROM ] && echo "E: romfile not specified!" && return -1
            [ ! -f $GVTD_ROM ] && echo "E: $GVTD_ROM not exists" && return -1
            GUEST_VGA_DEV="-device vfio-pci,host=00:02.0,x-igd-gms=2,id=gvtd_dev,bus=pcie.0,addr=0x2,x-igd-opregion=on,romfile=$GVTD_ROM"
        else
            echo "E: GVT-d Invalid parameters"
            return -1
        fi
        echo "setup GVT-d"
        sudo sh -c "modprobe vfio"
        sudo sh -c "modprobe vfio_pci"
        local intel_vga_pci_id=$(lspci -nn | grep "02.0 VGA" | grep -o "8086:....")
        if [ ! -z $intel_vga_pci_id ]; then
            sudo sh -c "echo 0000:00:02.0 > /sys/bus/pci/devices/0000:00:02.0/driver/unbind"
            sudo sh -c "echo ${intel_vga_pci_id/:/ } > /sys/bus/pci/drivers/vfio-pci/new_id"
            GUEST_DISP_TYPE="-display none"
        fi
    elif [[ $1 =~ ^GVT-g ]]; then
        local vgpu_uuid
        if [[ $1 == "GVT-g" ]]; then
            vgpu_uuid=""
        elif [[ $1 =~ ^GVT-g,uuid=* ]]; then
            vgpu_uuid=${1#*=}
        else
            echo "E: GVT-g Invald parameters"
            return -1
        fi
        create_gvtg_vgpu GVTG_VGPU_SYS_DEV "$vgpu_uuid"
        GUEST_VGA_DEV="-device vfio-pci-nohotplug,ramfb=on,sysfsdev=$GVTG_VGPU_SYS_DEV,display=on,x-igd-opregion=on"
    elif [[ $1 == "VirtIO" ]]; then
        GUEST_VGA_DEV="-device virtio-gpu-pci"
    elif [[ $1 == "QXL" ]]; then
        GUEST_VGA_DEV="-device qxl-vga,xres=480,yres=360"
    else
        echo "E: VGPU only support VirtIO,GVT-g,GVT-d,QXL. $1 is not supported"
        return -1
    fi
}

function set_fwd_port() {
    OIFS=$IFS IFS=',' port_arr=($1) IFS=$OIFS
    for e in "${port_arr[@]}"; do
        if [[ $e =~ ^adb= ]]; then
            GUEST_NET="${GUEST_NET/5555-:5555/${e#*=}-:5555}"
        elif [[ $e =~ ^fastboot= ]]; then
            GUEST_NET="${GUEST_NET/5554-:5554/${e#*=}-:5554}"
        else
            echo "E: Forward port, Invalid parameter"
            return -1;
        fi
    done
}

function set_block_dev() {
    [ ! -z $1 ] && GUEST_BLK_DEV+="-drive file=$1,format=raw"
}

function set_extra_qcmd() {
    GUEST_EXTRA_QCMD=$1
}

function set_pt_pci_vfio() {
    local PT_PCI=$1
    local unset=$2
    if [ ! -z $PT_PCI ]; then
        sudo sh -c "modprobe vfio-pci"
        local iommu_grp_dev="/sys/bus/pci/devices/$PT_PCI/iommu_group/devices/*"
        for d in $iommu_grp_dev; do
            local pci=$(basename $d)
            local vendor=$(cat $d/vendor)
            local device=$(cat $d/device)

            if [[ $unset == "unset" ]]; then
                echo "unset PCI passthrough: $pci, $vendor:$device"
                sudo sh -c "echo $pci > /sys/bus/pci/drivers/vfio-pci/unbind"
                sudo sh -c "echo $vendor $device > /sys/bus/pci/drivers/vfio-pci/remove_id"
                sudo sh -c "echo $pci > /sys/bus/pci/drivers_probe"
            else
                echo "set PCI passthrough: $pci, $vendor:$device"
                [[ -d $d/driver ]] && sudo sh -c "echo $pci > $d/driver/unbind"
                sudo sh -c "echo $vendor $device > /sys/bus/pci/drivers/vfio-pci/new_id"
            fi
        done
    fi
}

function set_pt_usb() {
    local USB_PCI=$(lspci -D |grep "USB controller" | grep -v "Thunderbolt" | grep -o "....:..:..\..")
    echo "passthrough USB device: $USB_PCI"

    if [[ $1 == "unset" ]]; then
        set_pt_pci_vfio $USB_PCI "unset"
    else
        set_pt_pci_vfio $USB_PCI
        GUEST_USB_PT_DEV=" -device vfio-pci,host=${USB_PCI#*:},x-no-kvm-intx=on"
    fi
}

function set_pt_audio() {
    local AUDIO_PCI=$(lspci -D |grep -i "Audio" | grep -o "....:..:..\..")
    echo "passthrough Aduio device: $AUDIO_PCI"

    if [[ $1 == "unset" ]]; then
        set_pt_pci_vfio $AUDIO_PCI "unset"
    else
        set_pt_pci_vfio $AUDIO_PCI
        GUEST_AUDIO_PT_DEV=" -device vfio-pci,host=${AUDIO_PCI#*:},x-no-kvm-intx=on"
        GUEST_AUDIO_DEV=""
    fi
}

function set_pt_eth() {
    local ETH_PCI=$(lspci -D |grep -i "Ethernet" | grep -o "....:..:..\..")
    echo "passthrough Ethernet device: $AUDIO_PCI"

    if [[ $1 == "unset" ]]; then
        set_pt_pci_vfio $ETH_PCI "unset"
    else
        set_pt_pci_vfio $ETH_PCI
        GUEST_ETH_PT_DEV=" -device vfio-pci,host=${ETH_PCI#*:},x-no-kvm-intx=on"
    fi
}

function set_pt_wifi() {
    local WIFI_PCI=$(lspci -D |grep -i "Network controller.* Wireless" | grep -o "....:..:..\..")
    echo "passthrough WiFi device: $WIFI_PCI"

    if [[ $1 == "unset" ]]; then
        set_pt_pci_vfio $WIFI_PCI "unset"
    else
        set_pt_pci_vfio $WIFI_PCI
        GUEST_WIFI_PT_DEV=" -device vfio-pci,host=${ETH_PCI#*:}"
    fi
}

function set_guest_pm() {
    local guest_pm_ctrl_daemon=$SCRIPTS_DIR/guest_pm_control
    if [ -f $guest_pm_ctrl_daemon ]; then
        local guest_pm_qmp_sock=$WORK_DIR/qmp-pm-sock
        $guest_pm_ctrl_daemon $guest_pm_qmp_sock &
        GUEST_PM_CTRL="-qmp unix:$guest_pm_qmp_sock,server,nowait -no-reboot"
        $guest_pm_ctrl_daemon "$guest_pm_ctrl_pipe" &
    fi
}

function set_guest_time_keep() {
    local guest_time_keep_daemon=$SCRIPTS_DIR/guest_time_keeping.sh
    if [ -f $guest_keep_daemon ]; then
        local guest_time_keep_pipe=$WORK_DIR/qmp-time-keep-pipe
        $guest_time_keep_daemon "$guest_time_keep_pipe" &
        GUEST_TIME_KEEP="-qmp pipe:$guest_time_keep_pipe"
    fi
}

function cleanup() {
    cleanup_rpmb_dev
    cleanup_thermal_mediation
    cleanup_battery_mediation
    [[ -z $GUEST_USB_PT_DEV ]] || set_pt_usb unset
    [[ -z $GUEST_AUDIO_PT_DEV ]] || set_pt_audio unset
}

function error() {
    echo "$BASH_SOURCE Failed at line $1: $2"
    exit
}

function launch_guest() {
    local EXE_CMD="$EMULATOR_PATH \
                   $GUEST_MEM \
                   $GUEST_CPU_NUM \
                   $GUEST_DISP_TYPE \
                   $GUEST_VGA_DEV \
                   $GUEST_RPMB_DEV \
                   $GUEST_DISK \
                   $GUEST_FIRMWARE \
                   $GUEST_VSOCK \
                   $GUEST_SHARE_FOLDER \
                   $GUEST_NET \
                   $GUEST_BLK_DEV \
                   $GUEST_AUDIO_DEV \
                   $GUEST_USB_PT_DEV \
                   $GUEST_AUDIO_PT_DEV \
                   $GUEST_ETH_PT_DEV \
                   $GUEST_WIFI_PT_DEV \
                   $GUEST_PM_CTRL \
                   $GUEST_TIME_KEEP \
                   $GUSET_VTPM \
                   $GUEST_STATIC_OPTION \
                   $GUEST_EXTRA_QCMD \
    "

    echo $EXE_CMD
    $EXE_CMD
}

function show_help() {
    printf "$(basename "$0") [-h] [-m] [-c] [-g] [-d] [-f] [-v] [-s] [-p] [-b] [-e] [--passthrough-pci-usb] [--passthrough-pci-audio] [--passthrough-pci-eth] [--passthrough-wifi] [--thermal-mediation] [--battery-mediation] [--guest-pm-control] [--guest-time-keep]\n"
    printf "Options:\n"
    printf "\t-h  show this help message\n"
    printf "\t-m  specify guest memory size, eg. \"-m 4G\"\n"
    printf "\t-c  specify guest cpu number, eg. \"-c 4\"\n"
    printf "\t-g  specify guest graphics mode, current support VirtIO|GVT-g|GVT-d|QXL.\n"
    printf "\t\tThe default value is VirtIO.\n"
    printf "\t\tVirtIO GPU, eg. \"-g VirtIO\"\n"
    printf "\t\tQXL VGA, eg. \"-g QXL\"\n"
    printf "\t\tGVT-g, eg. \"-g GVT-g,uuid=4ec1ff92-81d7-11e9-aed4-5bf6a9a2bb0a\", if uuid is not specified, a hardcoded uuid will be used\n"
    printf "\t\tGVT-d: romfile is supported for GVT-d, eg. \"-g GVT-d,romfile=/path/to/romfile\"\n"
    printf "\t-d  specify guest virtual disk image, eg. \"-d /path/to/android.img\"\n"
    printf "\t-f  specify guest firmware image, eg. \"-d /path/to/ovmf.fd\"\n"
    printf "\t-v  specify guest vsock cid, eg. \"-v 4\"\n"
    printf "\t-s  specify guest share folder path, eg. \"-s /path/to/share/with/guest\"\n"
    printf "\t-p  specify host forward ports, current support adb/fastboot, eg. \"-p adb=6666,fastboot=7777\"\n"
    printf "\t-b  specify host block device as guest virtual device, eg.\" -b /dev/mmcblk0 \"\n"
    printf "\t-e  specify extra qemu cmd, eg. \"-e \"-full-screen -monitor stdio\"\"\n"
    printf "\t--passthrough-pci-usb passthrough USB PCI bus to guest.\n"
    printf "\t--passthrough-pci-audio passthrough Audio PCI bus to guest.\n"
    printf "\t--passthrough-pci-eth passthrough Ethernet PCI bus to guest.\n"
    printf "\t--passthrough-pci-wifi passthrough WiFi PCI bus to guest.\n"
    printf "\t--thermal-mediation enable thermal mediation.\n"
    printf "\t--battery-mediation enable battery mediation.\n"
    printf "\t--guest-pm-control allow guest control host PM.\n"
    printf "\t--guest-time-keep reflect guest time setting on Host OS.\n"
}

function parse_arg() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|-\?|--help)
                show_help
                exit
                ;;

            -m)
                set_mem $2
                shift
                ;;

            -c)
                set_cpu $2
                shift
                ;;

            -g)
                set_graphics $2 || return -1
                shift
                ;;

            -d)
                set_disk $2
                shift
                ;;

            -f)
                set_firmware_path $2
                shift
                ;;

            -v)
                set_vsock $2
                shift
                ;;

            -s)
                set_share_folder $2
                shift
                ;;

            -p)
                set_fwd_port $2
                shift
                ;;

            -b)
                set_block_dev $2
                shift
                ;;

            -e)
                set_extra_qcmd "$2"
                shift
                ;;

            --passthrough-pci-usb)
                set_pt_usb
                ;;

            --passthrough-pci-audio)
                set_pt_audio
                ;;

            --passthrough-pci-eth)
                set_pt_eth
                ;;

            --passthrough-pci-wifi)
                set_pt_wifi
                ;;

            --thermal-mediation)
                setup_thermal_mediation
                ;;

            --battery-mediation)
                setup_battery_mediation
                ;;

            --guest-pm-control)
                set_guest_pm
                ;;

            --guest-time-keep)
                set_guest_time_keep
                ;;

            -?*)
                echo "Error: Invalid option $1"
                show_help
                return -1
                ;;
            *)
                echo "unknown option: $1"
                return -1
                ;;
        esac
        shift
    done
}


#-------------    main processes    -------------

trap 'cleanup' EXIT
trap 'error ${LINENO} "$BASH_COMMAND"' ERR

parse_arg "$@" || exit -1

check_kernel_version || exit -1
check_nested_vt || exit -1

setup_rpmb_dev || exit -1
setup_swtpm
setup_audio_dev || exit -1
launch_guest

echo "Done: \"$(realpath $0) $@\""
