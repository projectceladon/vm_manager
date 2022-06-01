# CIV vm_manager config doc

A [sample vm_manager config file](../sample/guest01.ini).

## All Groups

- global
- emulator
- memory
- vcpu
- firmware
- disk
- graphics
- vtpm
- rpmb
- aaf
- passthrough
- mediation
- guest_control
- extra

## [Passthrough]

PCIe devices to be passthroughed to the VM. 
Requirements: 
- Device name need to be full, such as "0000:00:14.0". 
- Device name must be listed in the output of `lspci` command. (`lspci` command must be able to produce a list of pci devices)
- Device names are separated by token "," (comma)

*Potential improvements*
- Detect whether `lspci` is available / Does not cause seg fault when `lspci` is not. 


## [Mediation]

Guest mediation services
Requirements:
- Executable `batsys` and `thermsys`


## [Guest control]

Guest services including time keeping and PM control
Requirements
- Absolute paths to executable services
- Write access to directory is required to create pipe and socket.

