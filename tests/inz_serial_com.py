import serial
import time
from dataclasses import dataclass
import struct
from func_timeout import func_set_timeout, FunctionTimedOut
from firebase import firebase

@dataclass
class Debug_frame:
    mode_status : int = 0
    current_time : int = 0
    heater_status : bool = False
    set_temp : float = 0.0
    enable : bool = False

class Serial_arduino:
    def __init__(self, com_port):
        self.ser = serial.Serial(com_port, 115200, timeout=None)
    
    def __del__(self):
        self.ser.close()
    
    @func_set_timeout(10)
    def read_last_frame(self) -> Debug_frame:
        self.ser.reset_input_buffer()
        debug_frame = Debug_frame()
        while True:
            if int.from_bytes(self.ser.read(),'big',signed=False) != 0x55:
                continue
            if int.from_bytes(self.ser.read(),'big',signed=False) != 0x5A:
                continue
            if int.from_bytes(self.ser.read(),'big',signed=False) != 0xAA:
                continue
            debug_frame.mode_status     = int.from_bytes(self.ser.read(),'big',signed=False)
            debug_frame.current_time    = int.from_bytes(self.ser.read(4), 'big', signed=False)
            debug_frame.heater_status   = True if int.from_bytes(self.ser.read(), 'big', signed=False) != 0 else False
            debug_frame.set_temp        = struct.unpack('>f', bytearray(self.ser.read(4)))[0]
            break
        return debug_frame

def main():
    ard = Serial_arduino('COM5')
    mode_list = ['NONE', 'MANUAL', 'AUTOMATIC', 'SCHEDULE']
    while True:
        frame = ard.read_last_frame()
        print(f'FRAME RECEIVED: ------------------------')
        print(f'MODE: {mode_list[frame.mode_status]}')
        print(f'Current time: {frame.current_time}')
        print(f'Set temp: {frame.set_temp}')
        print(f'Heater status: {frame.heater_status}')
        print(f'Enable: {frame.enable}')
        print(f'========================================')
        time.sleep(0.2)

if __name__ == "__main__":
    main()
