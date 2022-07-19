#include "FatTools.h"
#include "ff.h"
#include "ExtFlash.h"
#include <cstring>

FatTools fatTools;

// Create cache for header part of Fat
uint8_t fatCache[flashBlockSize * flashHeaderSize];			// Header consists of 1 block boot sector; 31 blocks FAT; 4 blocks Root Directory

uint8_t fsWork[flashBlockSize];								// Work buffer for the f_mkfs()
ExtFlash extFlash;											// Singleton external flash handler
FATFS fatFs;												// File system object for RAM disk logical drive
const char fatPath[4] = "0:/";								// Logical drive path for FAT File system

void FatTools::InitFatFS()
{
	// Set up cache area for header data
	memcpy(fatCache, flashAddress, flashBlockSize * flashHeaderSize);

	__attribute__((unused)) FRESULT res = f_mount(&fatFs, fatPath, 1) ;				// Register the file system object to the FatFs module

	if (res == FR_NO_FILESYSTEM) {
		MKFS_PARM parms;									// Create parameter struct
		parms.fmt = FM_FAT | FM_SFD;						// format as FAT12/16 using SFD (Supper Floppy Drive)
		parms.n_root = 64;									// Number of root directory entries (each uses 32 bytes of storage)
		parms.align = 0;									// Default initialise remaining values
		parms.au_size = 0;
		parms.n_fat = 0;

		f_mkfs(fatPath, &parms, fsWork, sizeof(fsWork));	// Mount FAT file system on External Flash
		res = f_mount(&fatFs, fatPath, 1);					// Register the file system object to the FatFs module
	}
}


uint8_t FatTools::FlushCache()
{
	// Writes any dirty cache sectors to Flash
	uint8_t blockPos = 0;
	uint8_t count = 0;
	while (cacheDirty != 0) {
		if (cacheDirty & (1 << blockPos)) {
			if (extFlash.WriteData(blockPos * 4096, (uint32_t*)&(fatCache[blockPos * 4096]), 1024, true)) {
				printf("Written block %i\r\n", blockPos);
				++count;
			}
			cacheDirty &= ~(1 << blockPos);
		}
		++blockPos;
	}
	return count;
}


void FatTools::GetFileInfo()
{
	FATFileInfo* fatInfo = (FATFileInfo*)(fatCache + fatFs.dirbase * flashBlockSize);
	printf("\r\nAttrib Cluster Bytes    Created   Accessed Name\r\n-----------------------------------------------\r\n");
	while (fatInfo->name[0] != 0) {
		if (fatInfo->attr == 0xF) {							// Long file name
			FATLongFilename* lfn = (FATLongFilename*)fatInfo;
			printf("*LFN*  part %2i                             %s\r\n",
					lfn->order & (~0x40),
					GetFileName(fatInfo).c_str());
		} else {
			printf("%s %8i %5lu %10s %10s %s\r\n",
					GetAttributes(fatInfo).c_str(),
					fatInfo->firstClusterLow,
					fatInfo->fileSize,
					FileDate(fatInfo->createDate).c_str(),
					FileDate(fatInfo->accessedDate).c_str(),
					GetFileName(fatInfo).c_str());
		}
		fatInfo++;
	}
}


std::string FatTools::FileDate(uint16_t date)
{
	// Bits 0–4: Day of month, valid value range 1-31 inclusive.
	// Bits 5–8: Month of year, 1 = January, valid value range 1–12 inclusive.
	// Bits 9–15: Count of years from 1980, valid value range 0–127 inclusive (1980–2107).
	return  std::to_string( date & 0b0000000000011111) + "/" +
			std::to_string((date & 0b0000000111100000) >> 5) + "/" +
			std::to_string(((date & 0b1111111000000000) >> 9) + 1980);
}


std::string FatTools::GetFileName(FATFileInfo* fi)
{
	char cs[14];
	std::string s;
	if (fi->attr == 0xF) {
		FATLongFilename* lfn = (FATLongFilename*)fi;
		return std::string({lfn->name1[0], lfn->name1[2], lfn->name1[4], lfn->name1[6], lfn->name1[8],
				lfn->name2[0], lfn->name2[2], lfn->name2[4], lfn->name2[6], lfn->name2[8], lfn->name2[10],
				lfn->name3[0], lfn->name3[2], '\0'});
	} else {
		uint8_t pos = 0;
		for (uint8_t i = 0; i < 11; ++i) {
			if (fi->name[i] != 0x20) {
				cs[pos++] = fi->name[i];
			} else if (fi->name[i] == 0x20 && cs[pos - 1] == '.') {
				// do nothing
			} else {
				cs[pos++] = (fi->attr & AM_DIR) ? '\0' : '.';
			}

		}
		cs[pos] = '\0';
		return std::string(cs);
	}
}


std::string FatTools::GetAttributes(FATFileInfo* fi)
{
	char cs[6] = {(fi->attr & AM_RDO) ? 'R' : '-',
			(fi->attr & AM_HID) ? 'H' : '-',
			(fi->attr & AM_SYS) ? 'S' : '-',
			(fi->attr & AM_DIR) ? 'D' : '-',
			(fi->attr & AM_ARC) ? 'A' : '-', '\0'};
	std::string s = std::string(cs);
	return s;
}


void FatTools::InvalidateFATCache()
{
	// Clear the cache window in the fatFS object so that new writes will be correctly read
	fatFs.winsect = ~0;
}


void FatTools::PrintFiles(char* path)						// Start node to be scanned (also used as work area)
{
	DIR dirObj;												// Pointer to the directory object structure
	FILINFO fileInfo;										// File information structure

	FRESULT res = f_opendir(&dirObj, path);					// second parm is directory name (root)

	if (res == FR_OK) {
		for (;;) {
			res = f_readdir(&dirObj, &fileInfo);			// Read a directory item */
			if (res != FR_OK || fileInfo.fname[0] == 0) {	// Break on error or end of dir */
				break;
			}
			if (fileInfo.fattrib & AM_DIR) {				// It is a directory
				uint32_t i = strlen(path);
				sprintf(&path[i], "/%s", fileInfo.fname);
				PrintFiles(path);							// Enter the directory
				path[i] = 0;
			} else {										// It is a file
				printf("%s/%s %i bytes\n", path, fileInfo.fname, (int)fileInfo.fsize);
			}
		}
		f_closedir(&dirObj);
	}
}

/*
void CreateTestFile()
{
	FIL MyFile;												// File object

	uint32_t byteswritten, bytesread;						// File write/read counts
	uint8_t wtext[] = "This is STM32 working with FatFs";	// File write buffer
	uint8_t rtext[100];										// File read buffer

	Create and Open a new text file object with write access
	if (f_open(&MyFile, "STM32.TXT", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK) {
		res = f_write(&MyFile, wtext, sizeof(wtext), (unsigned int*)&byteswritten);			// Write data to the text file
		if ((byteswritten != 0) && (res == FR_OK)) {
			f_close(&MyFile);																// Close the open text file
			if (f_open(&MyFile, "STM32.TXT", FA_READ) == FR_OK) {							// Open the text file object with read access
				res = f_read(&MyFile, rtext, sizeof(rtext), (unsigned int *)&bytesread);	// Read data from the text file
				if ((bytesread > 0) && (res == FR_OK)) {
					f_close(&MyFile);														// Close the open text file
				}
			}
		}
	}
}
*/
