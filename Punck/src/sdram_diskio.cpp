#include "ff_gen_drv.h"
#include "sdram_diskio.h"
#include "ExtFlash.h"
#include <cstring>

static volatile DSTATUS Stat = STA_NOINIT;
uint8_t fsWork[STORAGE_BLK_SIZ];			// a work buffer for the f_mkfs()


DSTATUS SDRAMDISK_initialize (BYTE);
DSTATUS SDRAMDISK_status (BYTE);
DRESULT SDRAMDISK_read (BYTE, BYTE*, DWORD, UINT);
DRESULT SDRAMDISK_write (BYTE, const BYTE*, DWORD, UINT);
DRESULT SDRAMDISK_ioctl (BYTE, BYTE, void*);

const Diskio_drvTypeDef  ExtFlashDriver = {
		SDRAMDISK_initialize,
		SDRAMDISK_status,
		SDRAMDISK_read,
		SDRAMDISK_write,
		SDRAMDISK_ioctl,
};

DSTATUS SDRAMDISK_initialize(BYTE lun)
{
	Stat = RES_OK;
	return Stat;
}

/**
 * @brief  Gets Disk Status
 * @param  lun : not used
 * @retval DSTATUS: Operation status
 */
DSTATUS SDRAMDISK_status(BYTE lun)
{
	return Stat;
}

/**
 * @brief  Reads Sector(s)
 * @param  lun : not used
 * @param  *buff: Data buffer to store read data
 * @param  sector: Sector address (LBA)
 * @param  count: Number of sectors to read (1..128)
 * @retval DRESULT: Operation result
 */
DRESULT SDRAMDISK_read(BYTE lun, BYTE* writeAddress, DWORD sector, UINT count)
{
	uint32_t writeSize = STORAGE_BLK_SIZ * count;
	uint32_t* readAddress = flashAddress + (sector * STORAGE_BLK_SIZ);

	memcpy((uint32_t*)writeAddress, readAddress, writeSize);

	return RES_OK;
}

/**
 * @brief  Writes Sector(s)
 * @param  lun : not used
 * @param  *buff: Data to be written
 * @param  sector: Sector address (LBA)
 * @param  count: Number of sectors to write (1..128)
 * @retval DRESULT: Operation result
 */
DRESULT SDRAMDISK_write(BYTE lun, const BYTE* buff, DWORD sector, UINT count)
{
	uint32_t words = (STORAGE_BLK_SIZ * count) / 4;
	uint32_t writeAddress = sector * STORAGE_BLK_SIZ;

	extFlash.WriteData(writeAddress, (uint32_t*)buff, words, true);

	return RES_OK;
}


/**
 * @brief  I/O control operation
 * @param  lun : not used
 * @param  cmd: Control code
 * @param  *buff: Buffer to send/receive control data
 * @retval DRESULT: Operation result
 */
DRESULT SDRAMDISK_ioctl(BYTE lun, BYTE cmd, void *buff)
{
	DRESULT res = RES_ERROR;

	if (Stat & STA_NOINIT) return RES_NOTRDY;

	switch (cmd) {
		// Make sure that no pending write process
		case CTRL_SYNC :
			res = RES_OK;
			break;

		// Get number of sectors on the disk (DWORD)
		case GET_SECTOR_COUNT :
			*(DWORD*)buff = STORAGE_BLK_NBR;
			res = RES_OK;
			break;

		// Get R/W sector size (WORD)
		case GET_SECTOR_SIZE :
			*(WORD*)buff = STORAGE_BLK_SIZ;
			res = RES_OK;
			break;

		// Get erase block size in unit of sector (DWORD)
		case GET_BLOCK_SIZE :
			*(DWORD*)buff = 1;		// FIXME - should be 4 (4 * 512 = 4096 which is the Flash sector erase size
			res = RES_OK;
			break;

		default:
			res = RES_PARERR;
	}

	return res;
}

