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
SERVICE_DIR=$AI_DISPATCHER_DIR/services/rawTensor
PROTO_FILE=nnhal_raw_tensor.proto
SERVER_FILE=$SERVICE_DIR/rawTensor.py

#---------      Functions      -------------------
function run_python_utility() {
    export PYTHONPATH="$PYTHONPATH:/home/$SUDO_USER/.local/bin:$AI_DISPATCHER_DIR"
    if [ ! -e "$SERVICE_DIR/$PROTO_FILE" ]; then
        echo "Not able to generate python files for the proto file"
        exit -1
    else
        python3 -m grpc_tools.protoc -I $SERVICE_DIR --python_out=$SERVICE_DIR --grpc_python_out=$SERVICE_DIR --proto_path=$SERVICE_DIR $PROTO_FILE
    fi

    if [ ! -f $SERVER_FILE ] && [ ! -f $LOAD_MODEL_FILE ] && [ ! -f $OVMS_INTERFACE_FILE ]; then
        echo "Python Bridge start script not avaliable"
        exit -1
    else
        echo "Ready for remote inferencing"
        if [[ ! -z "$1" ]]; then
                echo "start infer using device $1"
                python3 $SERVER_FILE --serving_mounted_modelDir $MODEL_DIR/ --interface ovtk --vsock true --remote 50059 --device $1
        else
                echo "start infer using AUTO"
                python3 $SERVER_FILE --serving_mounted_modelDir $MODEL_DIR/ --interface ovtk --vsock true --remote 50059
        fi
    fi
}

#-------------    main processes    -------------

run_python_utility $1

#!/bin/bash