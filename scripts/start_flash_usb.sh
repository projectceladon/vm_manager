#!/bin/bash

# Copyright (c) 2020 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

WORK_DIR=$PWD

[ $# -lt 1 ] && echo "Usage: $0 [caas-flashfiles-<variant>.zip] [caas-flashfile-<variant>.iso] [caas-flashfile-<variant>.iso.zip]" && exit -1

if [ -f android.qcow2 ]
then
	echo -n "android.qcow2 already exsited, Do you want to flash new one(y/N):"
	read option
	if [ "$option" == 'y' ]
	then
		rm android.qcow2
	else
		exit 1
	fi
fi

display_type="gtk,gl=on"
support_dedicated_data=false
for var in "$@"
do
	if [[ $var == "--display-off" ]]
	then
		display_type="none"
	elif [[ $var == "--dedicated-data" ]]
	then
		support_dedicated_data=true
	fi
done

if [ "$support_dedicated_data" = true ]
then
	qemu-img create -f qcow2 android.qcow2 8500M
else
	qemu-img create -f qcow2 android.qcow2 32G
fi

decompress=flashfiles_decompress
zip_file=$(file $1 | grep -i "Zip archive data")
if [[ $zip_file != "" ]]; then
	rm -rf "$decompress" && mkdir $decompress
	unzip $1 -d $decompress

	if [[ -f $decompress/boot.img ]]; then
		G_size=$((1<<32))
		for i in `ls $decompress`; do
			size=$(stat -c %s "$decompress/"$i)
			if [[ $size -gt $G_size ]]; then
				echo "Split $i due to its size bigger than 4G"
				split --bytes=$((G_size-1)) --numeric-suffixes "$decompress/"$i "$decompress/"$i.part
				rm "$decompress/"$i
			fi
		done

		dd if=/dev/zero of=./flash.vfat bs=63M count=160
		mkfs.vfat ./flash.vfat
		mcopy -i flash.vfat $decompress/* ::

		virt_disk=flash.vfat
	else
		virt_disk=$decompress/`ls $decompress`
	fi
else
	virt_disk=$1
fi

if [ "$support_dedicated_data" = true ]
then
	[ ! -d "./userdata" ] && mkdir ./userdata
	if [[ $(id -u) = 0 ]]
	then
		data_image=./userdata/$SUDO_USER.img
	else
		data_image=./userdata/$USER.img
	fi
	qemu-img create -f qcow2 $data_image 16G
fi

ovmf_file="./OVMF.fd"
[ ! -f $ovmf_file ] && ovmf_file="/usr/share/qemu/OVMF.fd"

#start software Trusted Platform Module
mkdir -p $WORK_DIR/vtpm0
swtpm socket --tpmstate dir=$WORK_DIR/vtpm0 --tpm2 --ctrl type=unixio,path=$WORK_DIR/vtpm0/swtpm-sock &

qemu-system-x86_64 \
  -m 2048 -smp 2 -M q35 \
  -name caas-vm \
  -enable-kvm \
  -k en-us \
  -vga qxl \
  -machine kernel_irqchip=on \
  -chardev socket,id=charserial0,path=./kernel-console,server=on,wait=off \
  -device isa-serial,chardev=charserial0,id=serial0 \
  -device qemu-xhci,id=xhci,addr=0x5 \
  -drive file=$virt_disk,id=udisk1,format=raw,if=none \
  -device usb-storage,drive=udisk1,bus=xhci.0 \
  -device virtio-scsi-pci,id=scsi0,addr=0x8 \
  -drive file=./android.qcow2,if=none,format=qcow2,id=scsidisk1 \
  -device scsi-hd,drive=scsidisk1,bus=scsi0.0 \
  -drive file=$ovmf_file,format=raw,if=pflash \
  -no-reboot \
  -display $display_type \
  -boot menu=on,splash-time=5000,strict=on \
  -chardev socket,id=chrtpm,path=$WORK_DIR/vtpm0/swtpm-sock \
  -tpmdev emulator,id=tpm0,chardev=chrtpm \
  -device tpm-crb,tpmdev=tpm0 \

echo "Flashing is completed"
