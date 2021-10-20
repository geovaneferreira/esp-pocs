# Python command line utility for OTA-UART example

This tool is used to transmit any compiled ESP32 image via a UART interface to the corresponding OTA software.

# Building

Install serial from pip
pip3 install pyserial

# Usage

Update you serial and .bin file in file update.py
ex
DIR_FW = 'dir to you folder/name_file.bin'
SER_PORT = '/dev/tty.usbserial-A50285BI' #or COM1 for windows...

# Run

Using your python3:

python3 update.py