# CIV vm_manager config doc

A [sample vm_manager config file](../sample/civ-1.ini).

## All Groups

* global
* emulator
* memory
* vcpu
* firmware
* disk
* graphics
* vtpm
* rpmb
* aaf
* passthrough
* mediation
* guest_control
* extra

## Configuration instructions


### [global]

requirements:
- name: name of the guest.
- flashfiles: path of flashfiles.

optional:
- vsock_cid: cid of VM.
- wait_ready: wait until vm is ready if `wait_ready == true`.


### [emulator]

Choose a emulator to start, only QEMU supported now.
requirements:
- path: path of the emulator.
optional:
- type: type of emulator, default is qemu.  


### [memory]

Configure guest RAM.
requirements:
- size: initial amount of guest memory.


### [vcpu]

requirements:
- size: specify the number of cores the guest is permitted to use.


### [firmware]

Add the pflash drive. Type must be "unified" or "splited"
requirements:
- type:
    * unified: 
        * path: path of virtual firmware binary. if `type == unified`, specify **path** field
    * splited: if `type == splited`, specify **code** and **vars** field
        * code: path of virtual firmware code binary. 
        * vars: path of virtual firmware vars binary.


### [disk]

Define a new drive and add storage device based on the driver.
requirements:
- size: the disk image size by bytes.
- path: path of disk image.


### [graphics]

Assign a virtual GPU to the virtual machine.
requirements:
- type: types of virtual GPU provided:
    -  virtio: a virtio based graphics adapter.
    -  ramfb: display device. Once the guest OS has initialized the vgpu qemu will show the vgpu display. Otherwise the ramfb framebuffer is used. 
    -  GVT-g: virtualize the GPU for guest, and still letting host use the virtualized GPU normally. When GVT-g is selected, below field must be specified.
       -  gvtg_version: the version of gvtg. 
       -  uuid: VGPU UUID.
    -  GVT-d: passthrough GPU to guest, host cannot use GPU.
- monitor: monitor id for SRIOV. 
- outputs: max outputs for virtio-gpu. 


### [display]

Specify the kind of display to use in QEMU. The GTK window is used by default if not specified.
options:
- options: choose one available display to use in QEMU.


### [net]

For network emulation.
requirements:
- model: optional ethernet model, default is e1000.
- adb_port: optional adb forwarding port.
- fastboot_port: optional fastboot forwarding port. 


### [vtpm]

Add Trusted Platform Module support to QEMU.
requirements:
- bin_path: path of vtpm binary.
- data_dir: path of data.


### [rpmb]

Create a device and expose one serial port to the guest.
requirements:
- bin_path: path of rpmb binary
- data_dir: path of the data.


### [aaf]

Share filesystem of host to the guest system.
requirements:
- path: The path to share.
- support_suspend: set the value to `true` or `enable` to enable suspend for guest. 
- audio_type:  must be one of these two options:
  - sof-hda: enable "sof-hda" only when "sof-hda" sound card is enabled in the host and you are making audio passthrough for guest. 
  - legacy-hda: In all other cases use "legacy-hda" only.


### [Passthrough]

PCIe devices to passthrough to the VM. Specified the PCI id here if you want to passthrough it to guest.
Requirements: 
- Device name need to be full, such as "0000:00:14.0". 
- Device name must be listed in the output of `lspci` command. (`lspci` command must be able to produce a list of pci devices)
- Device names are separated by token "," (comma)

*Potential improvements*
- Detect whether `lspci` is available / Does not cause seg fault when `lspci` is not. 


### [Mediation]

Guest mediation services. Executable `batsys` and `thermsys`
Requirements:
- battery_med
- thermal_med


### [Guest control]

Enable guest services. Absolute paths to executable services needed, and Write access to directory is required to create pipe and socket.
Requirements
- time_keep: absolute path to guest_time_keeping.sh.
- pm_control: absolute path to guest_pm_control.
- vinput: absolute path to vinput-manager.

### [audio]

- disable_emulation: Disable sound card emulation, set this option to true if the sound card is passthrough in `[passthrough]`

### [extra]
- cmd: Set extra command.
- service: Set extra services, use `:` split different services.
- pwr_ctrl_multios: If set to "true", will create qmp socket in /tmp/ folder for suspend/hibernate feature. Setting to "false" will not create the sockeet.

### [bluetooth]
- hci_down: If set to 'true', will bring down Bluetooth Hci Interface. Make sure BT USB Pci address is added in `[passthrough]`.
