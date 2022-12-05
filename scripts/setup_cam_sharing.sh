#!/bin/bash

# Copyright (c) 2020 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

function install_virtualcamera_service() {
    service_file=virtualcamera.service
    touch $service_file
    cat /dev/null > $service_file

    echo "[Unit]" > $service_file
    echo -e "Description=Virtual Camera Auto Start\n" >> $service_file
    echo -e "After=default.target\n" >> $service_file
    echo "[Service]" >> $service_file
    echo -e "ExecStartPre=/usr/sbin/modprobe v4l2loopback devices=2 video_nr=6,7 card_label=\"VCam0\",\"VCam1\" exclusive_caps=1,1\n" >> $service_file
    echo -e "ExecStart=/usr/bin/IntelCameraService -i /dev/video0 -o /dev/video6 -o /dev/video7 -w 1920 -h 1080 -f MJPG\n" >> $service_file
    echo -e "SuccessExitStatus=255\n" >> $service_file
    echo -e "Restart=always\n" >> $service_file
    echo -e "RestartSec=10\n" >> $service_file
    echo "[Install]" >> $service_file
    echo -e "WantedBy=default.target\n" >> $service_file

    cat $service_file
    sudo mv $service_file /etc/systemd/system/
    sudo systemctl enable virtualcamera.service
    sudo systemctl restart virtualcamera.service
}

function install_virtual_camera() {
    KERNELRELEASE=`uname -r`
    KERNEL_DIR=/lib/modules/${KERNELRELEASE}/kernel/drivers/media/v4l2-core/
    CURRENT_DIR=`pwd`

    sudo -E apt install linux-headers-`uname -r`

    echo "Clean environment..."
    if [ -x v4l2loopback ]; then
        rm -rf v4l2loopback
    fi
    IS_SERVICE_RUNNING=`ps -fe | grep IntelCameraService | grep -v "grep" | wc -l`
    if [ $IS_SERVICE_RUNNING == 1 ]; then
        echo "Stop running IntelCameraService"
        sudo systemctl stop virtualcamera.service
    fi
    IS_V4L2LOOPBACK_EXIST=`lsmod | grep "v4l2loopback" | wc -l`
    if [ $IS_V4L2LOOPBACK_EXIST != 0 ]; then
        echo "rmmod v4l2loopback..."
        sudo rmmod v4l2loopback
    fi

    echo "Install v4l2loopback driver..."
    git clone https://github.com/umlaeute/v4l2loopback.git
    cd v4l2loopback
    git checkout 81b8df79107d1fbf392fdcbaa051bd227a9c94c1

    git apply ../scripts/cam_sharing/0001-Netlink-sync.patch
    make

    echo "cp " ${DRIVER} ${KERNEL_DIR}/v4l2loopback.ko
    if [ -f ${KERNEL_DIR}/v4l2loopback.ko ]; then
        echo "Backup original v4l2loopback.ko"
        mv ${KERNEL_DIR}/v4l2loopback.ko ${KERNEL_DIR}/v4l2loopback.ko.orig
    fi
    sudo cp v4l2loopback.ko ${KERNEL_DIR}
    depmod
    cd ..
    if [ -x v4l2loopback ]; then
        rm -rf v4l2loopback
    fi

    echo "Install IntelCameraService in /usr/bin/ ..."
    sudo chmod 777 scripts/cam_sharing/IntelCameraService
    sudo cp ./scripts/cam_sharing/IntelCameraService /usr/bin/

    echo "Install virtualcamera.service in /lib/systemd/system/ ..."
    install_virtualcamera_service

    echo "Complete virtual camera installation."

}

function install_host_service() {
    sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavresample-dev libavdevice-dev -y
    sudo apt-get install ffmpeg -y
    sudo apt-get install build-essential clang -y

    sudo apt install git

    sudo apt-get install --yes cmake
    mkdir -p host_camera
    cd host_camera
    git clone https://github.com/projectceladon/host-camera-server.git
    cd host-camera-server
    git checkout 915388bd22d1582d44fb22dc647bd46a6ab675bc
    mkdir build
    cd build
    cmake ..
    cmake --build .
    sudo cp source/libvhal-client.so* /usr/lib
    sudo cp host_camera_service/stream /usr/local/bin/
    cd ../../..
    sudo rm -rf host_camera
    sudo apt install v4l-utils -y
    sudo v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080,pixelformat=MJPG
}


#-------------    main processes    -------------

install_host_service
install_virtual_camera

echo "Camera Sharing setup completed: \"$(realpath $0) $@\""
