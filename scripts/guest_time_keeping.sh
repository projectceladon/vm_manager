#!/bin/bash

# Copyright (c) 2020 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

# This script monitors qmp events and applies changes to the host.
# E.g. If CIV time/date is changed, the host time/date will be changed to the same value.

# QMP RTC_EVENT JSON Sample:
# {"timestamp": {"seconds": 1585714481, "microseconds": 85991}, "event": "RTC_CHANGE", "data": {"offset": -1}}

# QMP RTC_ALARM JSON Sample:
# {"timestamp": {"seconds": 1593413313, "microseconds": 568267}, "event": "RTC_ALARM", "data": {"expire": 1593499713567502000}}

# If guest time is set earlier than the image build time, the guest time will become the image build time after
# reboot

set -eE

#------------------------------------ Global Variables -----------------------------------------
QMP_PIPE=$1
RTC_MONITOR=$2

#------------------------------------     Functions    -----------------------------------------
function send_qmp_cmd() {
    if [[ -z $1 ]]; then
        echo "E: Empty parameter!"
        return -1
    fi

    if [[ -z $2 ]]; then
        echo "E: Empty command!"
        return -1
    fi

    local qmp_pipe_in=$1".in"
    local qmp_pipe_out=$1".out"
    local cmd='$2'

    if [[ ! -p $qmp_pipe_in || ! -p $qmp_pipe_out ]]; then
        echo "E: Not named pipe: $qmp_pipe_in $qmp_pipe_out"
        return -1
    fi

    echo "$2" > $qmp_pipe_in
    local result=
    read result < $qmp_pipe_out && echo $result
}

function connect_qmp_pipe() {
    local qmp_pipe=$1

    while true; do
       local out
       read out < $qmp_pipe".out" && echo $out
       if grep -q "{.*QMP.*version.*qemu.*package.*capabilities.*}" <<< "$out"; then
           break
       fi
       sleep 1
    done

    local result=$(send_qmp_cmd "$1" '{"execute":"qmp_capabilities"}')
    if [[ ${#result} -eq 15 && "${result:0:14}" == '{"return": {}}' ]]; then
        echo "QMP:$1 connected"
    else
        echo "Failed to connect QMP: $1"
        return -1
    fi
}

function create_pipe() {
    [[ -z $1 ]] && return
    [[ -p $1".in" ]] || mkfifo $1".in"
    [[ -p $1".out" ]] || mkfifo $1".out"
}

#------------------------------------     Main process    -----------------------------------------
create_pipe "$QMP_PIPE" || exit -1
connect_qmp_pipe "$QMP_PIPE" || exit -1
exec "$RTC_MONITOR" "$QMP_PIPE.in" "$QMP_PIPE.out"
