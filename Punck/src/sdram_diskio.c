#include "ff_gen_drv.h"
#include "sdram_diskio.h"

static volatile DSTATUS Stat = STA_NOINIT;
uint8_t fsWork[STORAGE_BLK_SIZ];			// a work buffer for the f_mkfs()
uint8_t virtualDisk[STORAGE_BLK_SIZ * STORAGE_BLK_NBR];			// RAM used as virtual disk
//extern uint8_t virtualDisk[STORAGE_BLK_SIZ * STORAGE_BLK_NBR];			// RAM used as virtual disk

DSTATUS SDRAMDISK_initialize (BYTE);
DSTATUS SDRAMDISK_status (BYTE);
DRESULT SDRAMDISK_read (BYTE, BYTE*, DWORD, UINT);
DRESULT SDRAMDISK_write (BYTE, const BYTE*, DWORD, UINT);
DRESULT SDRAMDISK_ioctl (BYTE, BYTE, void*);

const Diskio_drvTypeDef  SDRAMDISK_Driver = {
		SDRAMDISK_initialize,
		SDRAMDISK_status,
		SDRAMDISK_read,
		SDRAMDISK_write,
		SDRAMDISK_ioctl,
};

DSTATUS SDRAMDISK_initialize(BYTE lun)
{
//	Stat = STA_NOINIT;
//
//	// Configure the SDRAM device
//	if (BSP_SDRAM_Init() == SDRAM_OK) {
//		Stat &= ~STA_NOINIT;
//	}

	// Initialise virtual disk with 0xFF to mimic Flash storage
	memset(virtualDisk, 0xFF, STORAGE_BLK_SIZ * STORAGE_BLK_NBR);

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
DRESULT SDRAMDISK_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
	uint32_t *pSrcBuffer = (uint32_t *)buff;
	uint32_t BufferSize = (STORAGE_BLK_SIZ * count)/4;
	uint32_t *pSdramAddress = (uint32_t *) ((uint32_t)(&virtualDisk) + (sector * STORAGE_BLK_SIZ));

	for (; BufferSize != 0; BufferSize--) {
		*pSrcBuffer++ = *(volatile uint32_t *)pSdramAddress++;
	}

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
DRESULT SDRAMDISK_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
	uint32_t *pDstBuffer = (uint32_t *)buff;
	uint32_t BufferSize = (STORAGE_BLK_SIZ * count)/4;
	uint32_t *pSramAddress = (uint32_t *) ((uint32_t)(&virtualDisk) + (sector * STORAGE_BLK_SIZ));

	for (; BufferSize != 0; BufferSize--) {
		*(volatile uint32_t *)pSramAddress++ = *pDstBuffer++;
	}

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
			*(DWORD*)buff = 1;
			res = RES_OK;
			break;

		default:
			res = RES_PARERR;
	}

	return res;
}

