# GNU/Linux command line utility for OTA-UART example

This tool is used to transmit any compiled ESP32 image via a UART interface to the corresponding OTA software.

# Building

Compiled with gcc without further dependencies.
Install gcc on any Debian based system with 
`sudo apt install build-essentials gcc`

Compile with:

`gcc update.c -o update`


# Usage

`./update <ttyPort> <filename>`

e.g.,
`cd /media/psf/Home/Repositorios/GitHub\ Geovane/esp-pocs/POC05-OTA/update_commandline/`
`sudo ./update /dev/ttyUSB0 /media/psf/Home/Repositorios/GitHub\ Geovane/esp-pocs/POC05-OTA/update_commandline/POC02-freertos.bin`
