#teste
from time import sleep
from numpy.core.numeric import NaN

import serial

DIR_FW = '/Users/geovaneferreira/Repositorios/GitHub Geovane/esp-pocs/POC05-OTA/update_commandline/FW/POC02-freertos.bin'
SER_PORT = '/dev/tty.usbserial-A50285BI'
SER_BAUD = 115200

CHUNKSIZE = 32
CMD_START = [0x5a,0x55,0x5a,0x55]
CMD_END = [0x5F,0x5a,0x5F,0x5a]

def waitingAck():
    aguardarAck = 1
    while aguardarAck == 1:
        rcv = port.read(4)
        if rcv[0] == 0x1a and rcv[1] == 0x01 and rcv[2] == 0x79 and rcv[3] == 0x5d:
            print('Ack')
            aguardarAck = 0
            break
        else:
            sleep(0.2)

def waitingInit():
    aguardarAck = 1
    resendcmd = 0
    print('Send cmd to start')
    port.write(CMD_START)
    while aguardarAck == 1:
        rcv = port.read(4)
        if rcv[0] == 0x1a and rcv[1] == 0x01 and rcv[2] == 0x79 and rcv[3] == 0x5d:
            print('Ack')
            aguardarAck = 0
            break
        else:
            sleep(1)
            resendcmd = resendcmd + 1
            if resendcmd > 30:
                print('Resend cmd to start')
                resendcmd = 0
                port.write(CMD_START)


print("Starting UART ", SER_PORT)
port = serial.Serial(SER_PORT, baudrate=SER_BAUD)
print("--- UART ok ---\n")
sleep(1)

print("Opening FW ", DIR_FW)
file = open(DIR_FW, "rb")
print("--- FW ok ---")
sleep(1)

print("Waiting start")
waitingInit()
print("Starting send fw...")

totalsize = 0
finished = 0
bytes = [0x1]

while finished == 0:
    byteslido = file.read(CHUNKSIZE)
    port.write(byteslido)
    # hexadecimal_string = byteslido.hex()
    # print("Written: ", hexadecimal_string)

    if len(byteslido) == -1:
        print("error reading")
        exit()
        
    totalsize += len(byteslido)

    if len(byteslido) != CHUNKSIZE:
        print("Finished reading, total size: ",totalsize)
        sleep(2)
        port.write(CMD_END)
        finished = 1
    
    if (totalsize % 1024) == 0:
        print("Written: ",totalsize)
        waitingAck()

file.close()