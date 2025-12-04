// -----------------------------------------------------------------------------
// 3.5"/5.25" DD/HD Disk controller for Arduino
// Copyright (C) 2021 David Hansel
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
// -----------------------------------------------------------------------------

#include "ArduinoFDC.h"
#include "ff.h"

// -------------------------------------------------------------------------------------------------
// Basic helper functions
// -------------------------------------------------------------------------------------------------

#define TEMPBUFFER_SIZE (sizeof("w1,80,18,") + 1024 + 2)
byte tempbuffer[TEMPBUFFER_SIZE];

unsigned long motor_timeout = 0;

// from https://stackoverflow.com/a/34365963
static byte hextoint(char in) {
  int const x = in;
  return (x <= 57) ? x - 48 : (x <= 70) ? (x - 65) + 0x0a
                                        : (x - 97) + 0x0a;
}

void print_hex(byte b) {
  if (b < 16) Serial.write('0');
  Serial.print(b, HEX);
}


void dump_buffer(int offset, byte *buf, int n) {
  for (int i = 0; i < n; ++i) {
    print_hex(buf[i]);
  }
}


char *read_user_cmd(void *buffer, int buflen) {
  char *buf = (char *)buffer;
  uint16_t l = 0;
  do {
    int i = Serial.read();

    if ((i == 13 || i == 10)) {
      break;
    } else if (i == 27) {
      l = 0;
      break;
    } else if (i == 8) {
      if (l > 0) {
        l--;
      }
    } else if (isprint(i) && l < buflen - 1) {
      buf[l++] = i;
    }

    if (motor_timeout > 0 && millis() > motor_timeout) {
      ArduinoFDC.motorOff();
      motor_timeout = 0;
    }
  } while (true);

  while (l > 0 && isspace(buf[l - 1])) l--;
  buf[l] = 0;
  return buf;
}


bool confirm_formatting() {
  int c;
  Serial.print(F("Formatting will erase all data on the disk in drive "));
  Serial.write('A' + ArduinoFDC.selectedDrive());
  Serial.print(F(". Continue (y/n)?"));
  while ((c = Serial.read()) < 0)
    ;
  do { delay(1); } while (Serial.read() >= 0);
  Serial.println();
  return c == 'y';
}


void print_drive_type(byte n) {
  switch (n) {
    case ArduinoFDCClass::DT_5_DD: Serial.print(F("5.25\" DD")); break;
    case ArduinoFDCClass::DT_5_DDonHD: Serial.print(F("5.25\" DD disk in HD drive")); break;
    case ArduinoFDCClass::DT_5_HD: Serial.print(F("5.25\" HD")); break;
    case ArduinoFDCClass::DT_3_DD: Serial.print(F("3.5\" DD")); break;
    case ArduinoFDCClass::DT_3_HD: Serial.print(F("3.5\" HD")); break;
    default: Serial.print(F("Unknown"));
  }
}


void print_error(byte n) {
  Serial.print(F("Error: "));
  switch (n) {
    case S_OK: Serial.print(F("No error")); break;
    case S_NOTINIT: Serial.print(F("ArduinoFDC.begin() was not called")); break;
    case S_NOTREADY: Serial.print(F("Drive not ready")); break;
    case S_NOSYNC: Serial.print(F("No sync marks found")); break;
    case S_NOHEADER: Serial.print(F("Sector header not found")); break;
    case S_INVALIDID: Serial.print(F("Data record has unexpected id")); break;
    case S_CRC: Serial.print(F("Data checksum error")); break;
    case S_NOTRACK0: Serial.print(F("No track 0 signal detected")); break;
    case S_VERIFY: Serial.print(F("Verify after write failed")); break;
    case S_READONLY: Serial.print(F("Disk is write protected")); break;
    default: Serial.print(F("Unknonwn error")); break;
  }
  Serial.println('!');
}


void set_drive_type(int n) {
  ArduinoFDC.setDriveType((ArduinoFDCClass::DriveType)n);
  Serial.print(F("Setting disk type for drive "));
  Serial.write('A' + ArduinoFDC.selectedDrive());
  Serial.print(F(" to "));
  print_drive_type(ArduinoFDC.getDriveType());
  Serial.println();
}


// Some versions of Arduino appear to have problems with the FRESULT type in the print_ff_error
// function - which reportedly can be fixed by putting a forward declaration here (thanks to rtrimbler!).
// I am not able to reproduce the error but adding a forward declaration can't hurt.
void print_ff_error(FRESULT fr);

void print_ff_error(FRESULT fr) {
  Serial.print(F("Error #"));
  Serial.print(fr);
  Serial.print(F(": "));
  switch (fr) {
    case FR_DISK_ERR: Serial.print(F("Low-level disk error")); break;
    case FR_INT_ERR: Serial.print(F("Internal error")); break;
    case FR_NOT_READY: Serial.print(F("Drive not ready")); break;
    case FR_NO_FILE: Serial.print(F("File not found")); break;
    case FR_NO_PATH: Serial.print(F("Path not found")); break;
    case FR_INVALID_NAME: Serial.print(F("Invalid path format")); break;
    case FR_DENIED: Serial.print(F("Directory full")); break;
    case FR_EXIST: Serial.print(F("File exists")); break;
    case FR_INVALID_OBJECT: Serial.print(F("Invalid object")); break;
    case FR_WRITE_PROTECTED: Serial.print(F("Disk is write protected")); break;
    case FR_INVALID_DRIVE: Serial.print(F("Invalid drive")); break;
    case FR_NOT_ENABLED: Serial.print(F("The volume has no work area")); break;
    case FR_NO_FILESYSTEM: Serial.print(F("Not a FAT file system")); break;
    case FR_MKFS_ABORTED: Serial.print(F("Format aborted due to error")); break;
    case FR_NOT_ENOUGH_CORE: Serial.print(F("Out of memory")); break;
    case FR_INVALID_PARAMETER: Serial.print(F("Invalid parameter")); break;
    default: Serial.print(F("Unknown")); break;
  }
  Serial.println();
}

// -------------------------------------------------------------------------------------------------
// Low-level disk monitor
// -------------------------------------------------------------------------------------------------



static char *databuffer = nullptr;

void monitor() {
  char cmd;
  int a1, a2, a3, head, track, sector, n;
  size_t numCharsRead;
  char *cmdPtr;

  // for writing
  bool firstNibble = true;
  byte tempByte = 0;
  size_t inIdx = 0;
  size_t outIdx = 1;


  while (true) {
    // Serial.print(F("\r\n\r\nCommand: "));
    // n = sscanf(read_user_cmd(tempbuffer, 512), "%c%i,%i,%i,%s", &cmd, &a1, &a2, &a3, &databuffer);
    cmdPtr = read_user_cmd(tempbuffer, TEMPBUFFER_SIZE);
    n = sscanf(cmdPtr, "%c%i,%i,%i%n", &cmd, &a1, &a2, &a3, &numCharsRead);
    if (cmd == 'w') {
      databuffer = cmdPtr + numCharsRead + 1;
    } else {
      databuffer = tempbuffer;
    }

    if (n <= 0 || isspace(cmd)) continue;

    if (cmd == 'r' && n >= 3) {
      ArduinoFDC.motorOn();
      track = a1;
      sector = a2;
      head = a3;
      if (head >= 0 && head < 2 && track >= 0 && track < ArduinoFDC.numTracks() && sector >= 1 && sector <= ArduinoFDC.numSectors()) {
        // Serial.print(F("Reading track ")); Serial.print(track);
        // Serial.print(F(" sector ")); Serial.print(sector);
        // Serial.print(F(" side ")); Serial.println(head);
        // Serial.flush();

        byte status = ArduinoFDC.readSector(track, head, sector, databuffer);
        if (status == S_OK) {
          dump_buffer(0, databuffer + 1, 512);
          Serial.println();
        } else {
          Serial.println("in read");
          print_error(status);
        }
      } else
        Serial.println(F("Invalid sector specification"));
      ArduinoFDC.motorOff();
    } else if (cmd == 'w' && n >= 3) {
      ArduinoFDC.motorOn();
      track = a1;
      sector = a2;
      head = a3;
      if (head >= 0 && head < 2 && track >= 0 && track < ArduinoFDC.numTracks() && sector >= 1 && sector <= ArduinoFDC.numSectors()) {
        // Serial.print(F("Writing and verifying track ")); Serial.print(track);
        // Serial.print(F(" sector ")); Serial.print(sector);
        // Serial.print(F(" side ")); Serial.println(head);
        // Serial.flush();

        // convert databuffer from hex to bytes in-place
        // todo: maybe use base64 instead of hex? a bit more efficient but encode/decode is slower. in read too
        // data needs to be in databuffer[1 -> 512], not [0 -> 511]
        while (outIdx < 513) {
          if (firstNibble) {
            tempByte |= hextoint(databuffer[inIdx++]) << 4;
            firstNibble = false;
          } else {
            tempByte |= hextoint(databuffer[inIdx++]);
            databuffer[outIdx++] = tempByte;
            tempByte = 0;
            firstNibble = true;
          }
        }
        outIdx = 1;
        inIdx = 0;

        byte status = ArduinoFDC.writeSector(track, head, sector, databuffer, true);
        if (status != S_OK) {
          Serial.println(F("Error:"));
          print_error(status);
        }
        // send 'z' when done writing
        Serial.println('z');
        Serial.flush();
      } else
        Serial.println(F("Invalid sector specification"));
      ArduinoFDC.motorOff();
    } else if (cmd == 'm') {
      if (n == 1) {
        Serial.print(F("Drive "));
        Serial.write('A' + ArduinoFDC.selectedDrive());
        Serial.print(F(" motor is "));
        Serial.println(ArduinoFDC.motorRunning() ? F("on") : F("off"));
      } else {
        Serial.print(F("Turning drive "));
        Serial.write('A' + ArduinoFDC.selectedDrive());
        Serial.print(F(" motor "));
        if (n == 1 || a1 == 0) {
          Serial.println(F("off"));
          ArduinoFDC.motorOff();
        } else {
          Serial.println(F("on"));
          ArduinoFDC.motorOn();
        }
      }
    } else if (cmd == 's') {
      if (n == 1) {
        Serial.print(F("Current drive is "));
        Serial.write('A' + ArduinoFDC.selectedDrive());
      } else {
        Serial.print(F("Selecting drive "));
        Serial.write(a1 > 0 ? 'B' : 'A');
        Serial.println();
        ArduinoFDC.selectDrive(n > 1 && a1 > 0);
        ArduinoFDC.motorOn();
      }
    } else if (cmd == 't' && n > 1) {
      set_drive_type(a1);
    } else if (cmd == 'f') {
      if (confirm_formatting()) {
        Serial.println(F("Formatting disk..."));
        byte status = ArduinoFDC.formatDisk(databuffer, n > 1 ? a1 : 0, n > 2 ? a2 : 255);
        if (status != S_OK) print_error(status);
        memset(databuffer, 0, 513);
      }
    }
    else if (cmd == 'h' || cmd == '?') {
      Serial.println(F("Commands (t=track (0-based), s=sector (1-based), h=head (0/1)):"));
      Serial.println(F("r t,s,h  Read sector to buffer and print buffer"));
      Serial.println(F("r        Read ALL sectors and print pass/fail"));
      Serial.println(F("w t,s,h  Write buffer to sector"));
      Serial.println(F("w [0/1]  Write buffer to ALL sectors (without/with verify)"));
      Serial.println(F("b        Print buffer"));
      Serial.println(F("B [n]    Fill buffer with 'n' or 00..FF if n not given"));
      Serial.println(F("m 0/1    Turn drive motor off/on"));
      Serial.println(F("s 0/1    Select drive A/B"));
      Serial.println(F("t 0-4    Set type of current drive (5.25DD/5.25DDinHD/5.25HD/3.5DD/3.5HD)"));
      Serial.println(F("f        Low-level format disk (tf)"));
    }
    else
      Serial.println(F("Invalid command"));
  }
}


// -------------------------------------------------------------------------------------------------
// Main functions
// -------------------------------------------------------------------------------------------------


void setup() {
  Serial.begin(115200);
  ArduinoFDC.begin(ArduinoFDCClass::DT_3_HD, ArduinoFDCClass::DT_3_HD);
}


void loop() {
  monitor();
}
