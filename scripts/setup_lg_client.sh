#!/bin/bash

function ubu_install_lg_client(){
    sudo apt install -y binutils-dev cmake fonts-freefont-ttf libsdl2-dev libsdl2-ttf-dev libspice-protocol-dev libfontconfig1-dev libx11-dev nettle-dev
    touch /dev/shm/looking-glass0 && chmod 660 /dev/shm/looking-glass0
    touch /dev/shm/looking-glass1 && chmod 660 /dev/shm/looking-glass1
    touch /dev/shm/looking-glass2 && chmod 660 /dev/shm/looking-glass2
    touch /dev/shm/looking-glass3 && chmod 660 /dev/shm/looking-glass3
}

echo "set up lg client start"

ubu_install_lg_client

echo "set up lg client done"
