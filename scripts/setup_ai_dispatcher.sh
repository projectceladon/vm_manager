#!/bin/bash

# Copyright (c) 2023 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

set -eE

#---------      Global variable     -------------------
CIV_WORK_DIR=$(pwd)
AI_DISPATCHER_DIR=$CIV_WORK_DIR/ai-dispatcher
MODEL_DIR=$AI_DISPATCHER_DIR/model
CLIENT_REQ_FILE=client_requirements.txt
GRPC_PATH=$CIV_WORK_DIR/grpc

#---------      Functions      -------------------
function install_pre_requisites() {
    apt update
    apt-get install -y git python3 python-dev
    export PYTHONPATH="$HOME/.local/bin:${PYTHONPATH}"

    if [ ! -x "$(command -v pip3)" ]; then
        echo " Installing Python-pip"
        apt-get -y install python3-pip
    fi

    version=$(pip3 -V 2>&1 | grep -Po '(?<=pip )\d+')
    if [ "$version" -lt "23" ]; then
        echo "Upgrading Python-pip version"
        pip3 install --upgrade pip
    fi

    python3 -m pip install --ignore-installed six openvino-dev==2022.3.0
    echo "Installing pre-requisites"
}

function setup_ai_dispatcher() {
    echo "Setting up ai-dispatcher....."
    if [ ! -e "$AI_DISPATCHER_DIR" ]; then
        git clone https://github.com/projectceladon/ai-dispatcher.git $AI_DISPATCHER_DIR
    fi

    cd $AI_DISPATCHER_DIR
    if [ ! -e "$AI_DISPATCHER_DIR/$CLIENT_REQ_FILE" ]; then
        echo "Client Requirement file to enable remote inferencing not present"
        exit -1
    fi

    echo "Downloading client_requirement.txt"
    python3 -m pip install -r $AI_DISPATCHER_DIR/$CLIENT_REQ_FILE

    echo "create model directory"
    if [ -d "$MODEL_DIR" ]; then
        rm -rfd $MODEL_DIR
    fi
    mkdir -p $MODEL_DIR

    echo "Checking for environment PYTHON PATH"
    a=`grep -rn "PYTHONPATH\=\".*ai-dispatcher.*\"" /home/$SUDO_USER/.bashrc`
    b=`grep -rn "PYTHONPATH" /home/$SUDO_USER/.bashrc`
    if [ -z "$b" ]; then
        echo "export PYTHONPATH="$PYTHONPATH:$HOME/.local/bin:$AI_DISPATCHER_DIR"" | sudo tee -a $HOME/.bashrc
    elif [ -z "$a" ]; then
        sed -i "s|PYTHONPATH.*||g" $HOME/.bashrc
        echo "export PYTHONPATH="$PYTHONPATH:$HOME/.local/bin:$AI_DISPATCHER_DIR"" | sudo tee -a $HOME/.bashrc
    fi
    chown -R $SUDO_USER:$SUDO_USER $AI_DISPATCHER_DIR
}

function setup_grpc() {
    echo "Installing grpc with vsock support....."
    if [ ! -e "$GRPC_PATH" ]; then
        git clone -b v1.46.2 https://github.com/grpc/grpc.git $GRPC_PATH && cd $GRPC_PATH
        git submodule update --init
        echo "applying vsock patch"
        git apply -3<$CIV_WORK_DIR/patches/grpc-host/0001-support-vsock-v1.46.2.patch
        cd $GRPC_PATH/third_party/zlib
        echo "applying zlip patch"
        git apply -3<$CIV_WORK_DIR/patches/grpc-host/0002-Fix-a-bug-when-getting-a-gzip-header-extra-field-wit.patch
        cd $GRPC_PATH
    else
        cd $GRPC_PATH
        echo "grpc repo already cloned, skipping cloning..."
    fi
    pip3 install -r requirements.txt
    GRPC_PYTHON_BUILD_WITH_CYTHON=1 pip3 install .
    cd $CIV_WORK_DIR
}

function check_env_proxy() {
    a=`grep -rn "no_proxy\=\".*localhost.*\"" $HOME/.bashrc`
    b=`grep -rn "no_proxy" $HOME/.bashrc`
    echo "Checking for environment no_proxy settings"

    if [ -z "$b" ]; then
        echo "export no_proxy=\"localhost\"" | sudo tee -a $HOME/.bashrc
    elif [ -z "$a" ]; then
        sed -i "s|export no_proxy.*||g" $HOME/.bashrc
        echo "export no_proxy=\"$no_proxy,localhost\"" | sudo tee -a $HOME/.bashrc
    fi
}

function setup_gpu_drivers() {
    echo "setting up gpu drivers"
    sudo mkdir -p $CIV_WORK_DIR/neo && cd $CIV_WORK_DIR/neo
    wget https://github.com/intel/intel-graphics-compiler/releases/download/igc-1.0.13230.7/intel-igc-core_1.0.13230.7_amd64.deb
    wget https://github.com/intel/intel-graphics-compiler/releases/download/igc-1.0.13230.7/intel-igc-opencl_1.0.13230.7_amd64.deb
    wget https://github.com/intel/compute-runtime/releases/download/23.05.25593.11/intel-level-zero-gpu-dbgsym_1.3.25593.11_amd64.ddeb
    wget https://github.com/intel/compute-runtime/releases/download/23.05.25593.11/intel-level-zero-gpu_1.3.25593.11_amd64.deb
    wget https://github.com/intel/compute-runtime/releases/download/23.05.25593.11/intel-opencl-icd-dbgsym_23.05.25593.11_amd64.ddeb
    wget https://github.com/intel/compute-runtime/releases/download/23.05.25593.11/intel-opencl-icd_23.05.25593.11_amd64.deb
    wget https://github.com/intel/compute-runtime/releases/download/23.05.25593.11/libigdgmm12_22.3.0_amd64.deb
    dpkg -i *.deb
    cd $CIV_WORK_DIR
    rm -rfd $CIV_WORK_DIR/neo
}

function setup_infer_service() {
    echo "Adding AI-Dispatcher rawTensor Service"
    touch /lib/systemd/system/ai_raw_tensor.service

    cat << EOF | tee /lib/systemd/system/ai_raw_tensor.service
[Unit]
Description="Start/Stop Remote Inferencing"

[Service]
User=$SUDO_USER
WorkingDirectory=$(pwd)/
Type=simple
ExecStart=/bin/bash ./scripts/ai_raw_tensor.sh $1

[Install]
WantedBy=multi-user.target
EOF
    systemctl enable ai_raw_tensor
    systemctl daemon-reload
    systemctl start ai_raw_tensor
    echo "Starting Remote Inferencing Setup Script"
}

#-------------    main processes    -------------

check_env_proxy
install_pre_requisites
setup_ai_dispatcher
setup_grpc
if [ $1 =~ "GPU" -o $1 == "AUTO" ]; then
    setup_gpu_drivers
fi
setup_infer_service $1

echo "remote infer installation done"
#!/bin/bash