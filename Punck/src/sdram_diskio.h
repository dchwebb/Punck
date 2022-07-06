#pragma once
#define STORAGE_BLK_SIZ                  0x200		// Block Size is 512 Bytes
#define STORAGE_BLK_NBR                  128		// 200 * 512 = 102,400

#include "ff_gen_drv.h"

extern const Diskio_drvTypeDef  SDRAMDISK_Driver;
extern uint8_t fsWork[STORAGE_BLK_SIZ];			// a work buffer for the f_mkfs()
extern uint8_t virtualDisk[STORAGE_BLK_SIZ * STORAGE_BLK_NBR];			// RAM used as virtual disk
