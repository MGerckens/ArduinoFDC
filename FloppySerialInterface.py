import serial
import time
import re
from enum import IntEnum, auto

class FloppyErrorEnum(IntEnum):
    FR_DISK_ERR = 1
    FR_INT_ERR = auto()
    FR_NOT_READY = auto()
    FR_NO_FILE = auto()
    FR_NO_PATH = auto()
    FR_INVALID_NAME = auto()
    FR_DENIED = auto()
    FR_EXIST = auto()
    FR_INVALID_OBJECT = auto()
    FR_WRITE_PROTECTED = auto()
    FR_INVALID_DRIVE = auto()
    FR_NOT_ENABLED = auto()
    FR_NO_FILESYSTEM = auto()
    FR_MKFS_ABORTED = auto()
    FR_NOT_ENOUGH_CORE = auto()
    FR_INVALID_PARAMETER = auto()

class FloppyError(Exception):
    def __init__(self, code, text):
        self.code = code
        self.text = text

class FloppySerialInterface:
    INPUT_CMD_REGEX = re.compile(rb"^[A|B]:>")
    ERROR_REGEX = re.compile(rb"^Error #(\d): (.*)")

    def __init__(self, port: str, baud: int):
        self.ser = serial.Serial(port, baud, timeout=60)

    def sendCommand(self, cmd: bytes | str) -> list[bytes]:
        if isinstance(cmd, str):
            cmd = cmd.encode("ascii")
        if not cmd.endswith(b"\n"):
            cmd += b"\n"
        self.ser.write(cmd)
        gotInputReadback = False
        output = list()
        while not self.ser.readable():
            pass
        while True:
            line = self.ser.readline().rstrip(b"\r\n")
            print(line)
            if re.match(self.INPUT_CMD_REGEX, line):
                gotInputReadback = True
            elif gotInputReadback and line == b"":
                break
            elif m := re.match(self.ERROR_REGEX, line):
                raise FloppyError(m.group(1), m.group(2))
            else:
                output.append(line)

if __name__ == "__main__":
    interface = FloppySerialInterface("COM9", 2000000)
    time.sleep(5)
    interface.sendCommand("dir")
    print("done 1")
    time.sleep(1)
    interface.sendCommand("fulldir")
    print("done 2")
    interface.sendCommand("write test.txt\n\n")
    time.sleep(1)
    print("done 3")
