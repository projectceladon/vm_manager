Welcome to the developer documentation of vm manager. 



# Monitor guest console

1. To monitor guest console, add the console and log file for guest in the config file first with following command:

    ```
    [extra]
    cmd=-chardev socket,id=charserial0,path=console_path,server=on,wait=off,logfile=log_path -serial chardev:charserial0
    ```
    You need to specify paths for civ console and log file and replace ``console_path`` and ``log_path`` above. For example:

    ```
    [extra]
    cmd=-chardev socket,id=charserial0,path=/tmp/civ-console,server=on,wait=off,logfile=/tmp/civ_serial.log -serial chardev:charserial0
    ```
    Then you are able to find civ console named civ-console and log file named civ_serial.log under the /tmp.


2. Use the following command to monitor the guest console, the console_path shoud be the console path you specified before.

    ```sh
    $ socat file:$(tty),raw,echo=0 console_path
    ```


