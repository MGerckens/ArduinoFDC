#ifndef SERIALCOMMANDS_H_
#define SERIALCOMMANDS_H_

#include "ff.h"
#include "string.h"
#include "ArduinoFDC.h"

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


void listFullDir(FRESULT &fr, TCHAR *cmd, UINT &count, uint32_t *totalSize) {
  DIR dir;
  FILINFO finfo;

  ArduinoFDC.motorOn();
  const TCHAR* path = strlen(cmd) < 9 ? "0:\\" : cmd + 8;
  fr = f_opendir(&dir, path);
  if (fr == FR_OK) {
    count = 0;
    while (1) {
      fr = f_readdir(&dir, &finfo);
      if (fr != FR_OK || finfo.fname[0] == 0)
        break;

      if (finfo.fattrib & AM_DIR) {
        TCHAR *newCmd = new TCHAR[strlen(finfo.fname) + 8];
        strncpy(newCmd, "fulldir ", 9);
        strcat(newCmd, finfo.fname);
        listFullDir(fr, newCmd, count, totalSize);
        delete[] newCmd;
      } else {

        char *c = finfo.fname;
        byte col = 0;
        while (*c != 0 && *c != '.') {
          Serial.write(toupper(*c));
          col++;
          c++;
        }
        while (col < 9) {
          Serial.write(' ');
          col++;
        }
        if (*c == '.') {
          c++;
          while (*c != 0) {
            Serial.write(toupper(*c));
            col++;
            c++;
          }
        }
        while (col < 14) {
          Serial.write(' ');
          col++;
        }

        *totalSize += finfo.fsize;
        count++;

        Serial.println(finfo.fsize);
      }
    }
    
    f_closedir(&dir);
    
    // TCHAR volLabel[12];
    // f_getlabel("", volLabel, NULL);
    // Serial.print("Label: ");
    // Serial.println(volLabel);
    
    if (fr != FR_OK)
      print_ff_error(fr);
  } else
    print_ff_error(fr);
}

void listDir(FRESULT &fr, TCHAR *cmd, UINT &count) {
  DIR dir;
  FILINFO finfo;

  ArduinoFDC.motorOn();
  fr = f_opendir(&dir, strlen(cmd) < 5 ? "0:\\" : cmd + 4);
  if (fr == FR_OK) {
    count = 0;
    while (1) {
      fr = f_readdir(&dir, &finfo);
      if (fr != FR_OK || finfo.fname[0] == 0)
        break;

      char *c = finfo.fname;
      byte col = 0;
      while (*c != 0 && *c != '.') {
        Serial.write(toupper(*c));
        col++;
        c++;
      }
      while (col < 9) {
        Serial.write(' ');
        col++;
      }
      if (*c == '.') {
        c++;
        while (*c != 0) {
          Serial.write(toupper(*c));
          col++;
          c++;
        }
      }
      while (col < 14) {
        Serial.write(' ');
        col++;
      }
      if (finfo.fattrib & AM_DIR)
        Serial.println(F("<DIR>"));
      else
        Serial.println(finfo.fsize);
      count++;
    }

    f_closedir(&dir);

    if (fr == FR_OK) {
      if (count == 0) Serial.println(F("No files."));

      FATFS *fs;
      DWORD fre_clust;
      fr = f_getfree("0:", &fre_clust, &fs);

      if (fr == FR_OK) {
        Serial.print(fre_clust * fs->csize * 512);
        Serial.println(F(" bytes free."));
      }
    }

    if (fr != FR_OK)
      print_ff_error(fr);
  } else
    print_ff_error(fr);
}

void status(const TCHAR* cmd){
    FILINFO fno;
    TCHAR *path = cmd + 7;
    FRESULT res = f_stat(path, &fno);
    
    if(res != FR_OK){
        print_ff_error(res);
        return;
    }
    Serial.print("Size: ");
    Serial.println(fno.fsize);

    Serial.print("Last modified: ");
    uint8_t modifiedYear = ((fno.fdate & 0x1111111000000000u) >> 9u) + 1980u;
    uint8_t modifiedMonth = (fno.fdate & 0x0000000111100000u) >> 5u;
    uint8_t modifiedDay = fno.fdate & 0x0000000000011111u;
    uint8_t modifiedHour = (fno.ftime & 0x1111100000000000u) >> 11u;
    uint8_t modifiedMinute = (fno.ftime & 0x0000011111100000u) >> 5u;
    uint8_t modifiedSecond = (fno.ftime & 0x0000000000011111u) * 2u;
    Serial.print(modifiedYear);
    Serial.print("-");
    Serial.print(modifiedMonth);
    Serial.print("-");
    Serial.print(modifiedDay);
    Serial.print(" ");
    Serial.print(modifiedHour);
    Serial.print(":");
    Serial.print(modifiedMinute);
    Serial.print(":");
    Serial.println(modifiedSecond);

    Serial.print("Attributes: ");
    if(fno.fattrib & AM_RDO){
        Serial.print("Read-only ");
    }
    if(fno.fattrib & AM_HID){
        Serial.print("Hidden ");
    }
    if(fno.fattrib & AM_SYS){
        Serial.print("System ");
    }
    if(fno.fattrib & AM_ARC){
        Serial.print("Archive ");
    }
    if(fno.fattrib & AM_DIR){
        Serial.print("Directory");
    }
    Serial.println();
}

#endif