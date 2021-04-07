#!/bin/bash

function ubu_install_lg_client(){
    sudo apt install -y binutils-dev cmake fonts-freefont-ttf libsdl2-dev libsdl2-ttf-dev libspice-protocol-dev libfontconfig1-dev libx11-dev nettle-dev
    sudo touch /dev/shm/looking-glass0 && sudo chown root:kvm /dev/shm/looking-glass0 && sudo chmod 660 /dev/shm/looking-glass0
    sudo touch /dev/shm/looking-glass1 && sudo chown root:kvm /dev/shm/looking-glass1 && sudo chmod 660 /dev/shm/looking-glass1
    sudo touch /dev/shm/looking-glass2 && sudo chown root:kvm /dev/shm/looking-glass2 && sudo chmod 660 /dev/shm/looking-glass2
    sudo touch /dev/shm/looking-glass3 && sudo chown root:kvm /dev/shm/looking-glass3 && sudo chmod 660 /dev/shm/looking-glass3
}

echo "set up lg client start"

ubu_install_lg_client

echo "set up lg client done"
