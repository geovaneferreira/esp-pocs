import serial
from datetime import *
import time


class SerChannel(object):
    def __init__(self, port, baudrate, rxq):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.frame = ''
        self.rxq = rxq

    def open(self):
        try:
            self.ser = serial.Serial(port=self.port, baudrate=self.baudrate)
            self.ser.flush()
            # self.ser.setTimeout(20)
            # self.ser.setWriteTimeout(20)
        except Exception as e:
            print(repr(e))
            print('exception opening serial %s' % self.port)
            return False
        else:
            return True

    def close(self):
        try:
            self.ser.close()
        except Exception as e:
            print(repr(e))
            print('exception closing serial %s' % self.port)
            return False
        else:
            return True

    def send_break(self):
        self.ser.flushInput()
        self.ser.sendBreak()

    def send(self, buf, clear_input_buffer=False):
        if clear_input_buffer:
            self.ser.flushInput()

        self.send_cmd(buf)

    def send_cmd(self, cmd):
        n = self.ser.write(cmd)
        if n != len(cmd):
            print('write error %d != %d' % (n, len(cmd)))
        self.ser.flush()

    def recv(self, frame_tmrout=1.0):
        t1 = time.time()
        data = b''
        while True:
            nb = self.ser.inWaiting()
            if not nb:
                time.sleep(0.005)
            else:
                data = data + self.ser.read(nb)
                t1 = time.time()
                # break  # As we can receive data in pieces, this way is much faster

            t2 = time.time()
            if (t2 - t1) > frame_tmrout:
                break
        return data

    def dev_thread(self):
        try:
            print('Listen on serial port %s\n' % self.port)
            if not self.open():
                raise serial.SerialException
            while True:

                data = self.recv(0.02)
                if not data:
                    continue
                msg = {'CMD': 'serial_recv', 'DATA': data}
                self.rxq.put(msg)

        except:
            print('Serial Disconnected\n')
            msg = {'CMD': 'serial_error'}
            self.rxq.put(msg)
