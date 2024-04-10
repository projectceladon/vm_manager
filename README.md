
CiV VM Manager
=============

CiV VM Manager is a linux userspace application aimed to manage the CiV guests.

CiV: Celadon in Virtual Machine(https://01.org/projectceladon/about)

You can find the latest vm-manager release here: https://github.com/projectceladon/vm_manager/releases

  

# Architecture of VM Manager

![VM Manager Arch](/docs/arch.drawio.svg)


# Building

## Pre-requisites

UBUNTU OS Version: 20.04 or 22.04

Install required tools on Linux. 

```sh
 $ [sudo] apt-get install build-essential autoconf libtool pkg-config cmake
```

## Build

```sh
 $ mkdir build && cd build
 $ cmake -DCMAKE_BUILD_TYPE=Release ..
 $ cmake --build . --config Release
```

