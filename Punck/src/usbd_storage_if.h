#pragma once
#include "initialisation.h"
#include "ExtFlash.h"

#define STORAGE_LUN_NBR                  1			// Logical Unit Number
#define STORAGE_BLK_NBR                  50		// 200 * 512 = 102,400
//#define STORAGE_BLK_SIZ                  0x200		// Block Size is 512 Bytes.

extern uint8_t virtualDisk[STORAGE_BLK_NBR * flashBlockSize];

void STORAGE_Init_FS();
void STORAGE_GetCapacity_FS(uint32_t *block_num, uint16_t *block_size);
void STORAGE_Read_FS(uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
void STORAGE_Write_FS(uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
