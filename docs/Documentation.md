# User Guide

Verified on:

* Hardware: KBL-NUC/CML-NUL
* OS: Ubuntu20.04


# Getting Started

## Prerequisites setup for Host Environment

1. Create required directories under `$HOME/` .
    ```sh
    $ mkdir ~/caas
    $ mkdir ~/caas/aaf
    $ mkdir ~/caas/vtpm0
    ```

2. Download [celadon release](https://github.com/projectceladon/celadon-binary/) in `$HOME/caas/`. e.g. [CIV_00.21.01.12_A11](https://github.com/projectceladon/celadon-binary/tree/master/CIV_00.21.01.12_A11) .
    ```sh
    $ cd ~/caas/
    $ tar zxvf caas-releasefiles-${build_variant}.tar.gz
    ```

3. Launch the script provided to set up the environment for running CiV, **it may need reboot** .
    
    ```sh
    $ sudo -E ./scripts/setup_host.sh 
    ```


4. Modify the authentication of OVMF.fd .
    ```sh
    $ sudo chmod a+rw OVMF.fd
    ```
    


## Install vm-manager

Download [vm-manager package](https://github.com/projectceladon/vm_manager/suites/7205036575/artifacts/288276649), there should be a **vm-manager_1.0.0_amd64.deb** after unzip it.

```sh
$ sudo dpkg -i vm-manager_1.0.0_amd64.deb
```

### Usages of VM Manager
   
   ```
   $ vm-manager -h
    Usage:
        vm-manager [-c] [-d vm_name] [-b vm_name] [-q vm_name] [-f vm_name] [-u vm_name] [--get-cid vm_name] [-l] [-v] [-h]
    Options:
    -h [ --help ]         Show this help message
    -c [ --create ] arg   Create a CiV guest
    -d [ --delete ] arg   Delete a CiV guest
    -b [ --start ] arg    Start a CiV guest
    -q [ --stop ] arg     Stop a CiV guest
    -f [ --flash ] arg    Flash a CiV guest
    -u [ --update ] arg   Update an existing CiV guest
    --get-cid arg         Get cid of a guest
    -l [ --list ]         List existing CiV guest
    -v [ --version ]      Show CiV vm-manager version
    --start-server        Start host server
    --stop-server         Stop host server
    --daemon              start server as a daemon
   ```



## Create a new Civ


1. Create folder to store civ config file.
   
   ```sh
   $ mkdir -p ~/.intel/.civ/
   ```

2. Create an ini file under `$HOME/.intel/.civ/` .  
   Eg. If you want to name your civ instance as **civ-1**,the ini file `civ-1.ini` should be created under `$HOME/.intel/.civ/`.
   
    ```sh
    $ touch ~/.intel/.civ/civ-1.ini
    ```
    Follow the sample ini to config your civ instance: [civ-1.ini](../sample/civ-1.ini). For details, see [the specific instructions of configuration](fields.md).





## Start the instance

1. Start vm-manager server as daemon
    ```
    $ vm-manager –start-server –daemon 
    ```

2. Flash virtual disk if required, the flashing process will take a while. If the virtual disk image is already flashed, you can skip this step.
    ```
    $ vm-manager -f civ-1
    ```

3. Start a Civ guest
    ```
    $ vm-manager -b civ-1
    ```



## Other useful commands

1. Create a Civ guest  
   This command will bring up a terminal UI, you can find the corresponding ini file in `$HOME/.intel/.civ/` after you clicked save button.  
   Eg. If you want to name your civ instance as **civ-1**, the command should be:
   ```sh
   $ vm-manager -c civ-1
   ```
2. Import an existing config file
   ```sh
    $ vm-manager -i civ-2.ini
    ```
3. Delete a CiV config
    ```sh
    $ vm-manager -d civ-1
    ```

4. Update/Modify a CiV config
    ```sh
    $ vm-manager -u civ-1
    ```

5. List Guest
   ```sh
   $ vm-manager -b civ-1
   ```

6. Stop Guest  
    This command will force to quit the guest
    ```sh
    $ sudo vm-manager -q civ-1
    ```


