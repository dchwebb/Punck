#include "diskio.h"
#include "ExtFlash.h"
#include "FatTools.h"
#include <cstring>


// Wrapper functions to interface FatFS library with FAT and ExtFlash handlers
uint8_t disk_initialize(uint8_t pdrv)
{
	return RES_OK;
}


uint8_t disk_status(uint8_t pdrv)
{
	return RES_OK;
}


uint8_t disk_read(uint8_t pdrv, uint8_t* writeAddress, uint32_t readSector, uint32_t sectorCount)
{
	fatTools.Read(writeAddress, readSector, sectorCount);
	return RES_OK;
}


uint8_t disk_write(uint8_t pdrv, const uint8_t* readBuff, uint32_t writeSector, uint32_t sectorCount)
{
	fatTools.Write(readBuff, writeSector, sectorCount);
	return RES_OK;
}


uint8_t disk_ioctl(uint8_t pdrv, uint8_t cmd, void* buff)
{
	uint8_t res = RES_OK;

	switch (cmd) {
		case CTRL_SYNC:					// Make sure that no pending write process
			break;

		case GET_SECTOR_COUNT:			// Get number of sectors on the disk
			*(uint32_t*)buff = flashSectorCount;
			break;

		case GET_SECTOR_SIZE:			// Get R/W sector size
			*(uint32_t*)buff = flashSectorSize;
			break;

		case GET_BLOCK_SIZE:			// Get erase block size in unit of sector
			*(uint32_t*)buff = 8;		// 8 * 512 = 4096 which is the Flash sector erase size
			break;

		default:
			res = RES_PARERR;
	}

	return res;
}


uint32_t get_fattime()
{
	return 0;
}


