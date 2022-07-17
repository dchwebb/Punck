extern "C" {
#include "diskio.h"
}
#include "ExtFlash.h"
#include <cstring>

// Create cache for header part of Fat
uint8_t FatCache[flashBlockSize * flashHeaderSize];		// Header consists of 1 block boot sector; 31 blocks FAT; 4 blocks Root Directory

// Wrapper functions to interface FatFS library to ExtFlash handler
uint8_t disk_initialize(uint8_t pdrv)
{
	// Set up cache area for header
	memcpy(FatCache, flashAddress, flashBlockSize * flashHeaderSize);
	return RES_OK;
}


uint8_t disk_status(uint8_t pdrv)
{
	return RES_OK;
}


uint8_t disk_read(uint8_t pdrv, uint8_t* writeAddress, uint32_t readSector, uint32_t sectorCount)
{
	// If reading header data return from cache
	const uint8_t* readAddress;
	if (readSector < flashHeaderSize) {
		readAddress = &(FatCache[readSector * flashBlockSize]);
	} else {
		readAddress = ((uint8_t*)flashAddress) + (readSector * flashBlockSize);
	}

	memcpy(writeAddress, readAddress, flashBlockSize * sectorCount);
	return RES_OK;
}


uint8_t disk_write(uint8_t pdrv, const uint8_t* readBuff, uint32_t writeSector, uint32_t sectorCount)
{
	if (writeSector < flashHeaderSize) {
		uint8_t* writeAddress = &(FatCache[writeSector * flashBlockSize]);
		memcpy(writeAddress, readBuff, flashBlockSize * sectorCount);
	} else {
		uint32_t words = (flashBlockSize * sectorCount) / 4;
		uint32_t writeAddress = writeSector * flashBlockSize;
		extFlash.WriteData(writeAddress, (uint32_t*)readBuff, words, true);
	}

	return RES_OK;
}


uint8_t disk_ioctl(uint8_t pdrv, uint8_t cmd, void* buff)
{
	uint8_t res = RES_OK;

	switch (cmd) {
		case CTRL_SYNC:					// Make sure that no pending write process
			break;

		case GET_SECTOR_COUNT:			// Get number of sectors on the disk
			*(uint32_t*)buff = flashBlockCount;
			break;

		case GET_SECTOR_SIZE:			// Get R/W sector size
			*(uint32_t*)buff = flashBlockSize;
			break;

		case GET_BLOCK_SIZE:			// Get erase block size in unit of sector
			*(uint32_t*)buff = 4;		// Set to 4 (4 * 512 = 4096 which is the Flash sector erase size)
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
