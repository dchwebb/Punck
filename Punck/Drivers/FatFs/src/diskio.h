#pragma once
#include "stm32h7xx.h"

// Disk Status Bits (DSTATUS)
#define STA_NOINIT		0x01	// Drive not initialized
#define STA_NODISK		0x02	// No medium in the drive
#define STA_PROTECT		0x04	// Write protected

// Generic command (Used by FatFs)
#define CTRL_SYNC			0	// Complete pending write process (needed at _FS_READONLY == 0)
#define GET_SECTOR_COUNT	1	// Get media size (needed at _USE_MKFS == 1)
#define GET_SECTOR_SIZE		2	// Get sector size (needed at _MAX_SS != _MIN_SS)
#define GET_BLOCK_SIZE		3	// Get erase block size (needed at _USE_MKFS == 1)
#define CTRL_TRIM			4	// Inform device that the data on the block of sectors is no longer used (needed at _USE_TRIM == 1)

// Status of Disk Functions
typedef uint8_t	DSTATUS;

// Results of Disk Functions
typedef enum {
	RES_OK = 0,					// 0: Successful
	RES_ERROR,					// 1: R/W Error
	RES_WRPRT,					// 2: Write Protected
	RES_NOTRDY,					// 3: Not Ready
	RES_PARERR					// 4: Invalid Parameter
} DRESULT;


// The following functions are called from C FatFS library
#ifdef __cplusplus
extern "C" {
#endif

uint8_t disk_initialize (uint8_t pdrv);
uint8_t disk_status (uint8_t pdrv);
uint8_t disk_read (uint8_t pdrv, uint8_t *writeAddress, uint32_t readSector, uint32_t sectorCount);
uint8_t disk_write (uint8_t pdrv, const uint8_t *readBuff, uint32_t writeSector, uint32_t sectorCount);
uint8_t disk_ioctl (uint8_t pdrv, uint8_t cmd, void* buff);
uint32_t get_fattime();

#ifdef __cplusplus
}
#endif




