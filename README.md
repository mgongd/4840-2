# FPGA Setup
1. Connect workstation to the board via **MINI** USB (**NOT** the USB type-B)
2. On **workstation**, start the *screen* terminal emulator of the board
   ```shell
   screen /dev/ttyUSB0 115200
   ```
3. Power on the board
4. Login as *root* with password *CSee4840!*
5. Connect board to **ethernet**, monitor, and keyboard
6. Configure internet and terminal settings:
    ```shell
    ifup eth0
    stty rows 74
    stty cols 132
    ```
7. Go to `~/lab2` and run the program
   ```shell
   cd ~/lab2
   git pull
   make
   ./lab2
   ```
   and you'll be able to interact with the keyboard and monitor.
