#include "FatTools.h"
#include "ExtFlash.h"
#include <cstring>
#include "usb.h"

FatTools fatTools;


void FatTools::InitFatFS()
{
	// Set up cache area for header data
	memcpy(headerCache, flashAddress, fatSectorSize * fatCacheSectors);

	FRESULT res = f_mount(&fatFs, fatPath, 1) ;				// Register the file system object to the FatFs module

	if (res == FR_NO_FILESYSTEM) {
		uint8_t fsWork[fatSectorSize];						// Work buffer for the f_mkfs()

		MKFS_PARM parms;									// Create parameter struct
		parms.fmt = FM_FAT | FM_SFD;						// format as FAT12/16 using SFD (Supper Floppy Drive)
		parms.n_root = 128;									// Number of root directory entries (each uses 32 bytes of storage)
		parms.align = 0;									// Default initialise remaining values
		parms.au_size = 0;
		parms.n_fat = 0;

		f_mkfs(fatPath, &parms, fsWork, sizeof(fsWork));	// Mount FAT file system on External Flash
		res = f_mount(&fatFs, fatPath, 1);					// Register the file system object to the FatFs module

		// Populate Windows spam files to prevent them being created later in unwanted locations
		MakeDummyFiles();
	}

	// Remove the ClnShutBitMask where Windows assumes a badly shut down drive is corrupt
	uint32_t* cluster = (uint32_t*)(fatTools.headerCache + (fatTools.fatFs.fatbase * fatSectorSize));
	*cluster |= 0x80000000;

	UpdateSampleList();										// Updated list of samples on flash
}



void FatTools::Read(uint8_t* writeAddress, uint32_t readSector, uint32_t sectorCount)
{
	// If reading header data return from cache
	const uint8_t* readAddress;
	if (readSector < fatCacheSectors) {
		readAddress = &(headerCache[readSector * fatSectorSize]);
	} else {
		readAddress = flashAddress + (readSector * fatSectorSize);
	}

	memcpy(writeAddress, readAddress, fatSectorSize * sectorCount);
}


void FatTools::Write(const uint8_t* readBuff, uint32_t writeSector, uint32_t sectorCount)
{
	if (writeSector < fatCacheSectors) {
		// Update the bit array of dirty blocks [There are 8 x 512 byte sectors in a block (4096)]
		dirtyCacheBlocks |= (1 << (writeSector / fatEraseSectors));

		uint8_t* writeAddress = &(headerCache[writeSector * fatSectorSize]);
		memcpy(writeAddress, readBuff, fatSectorSize * sectorCount);
	} else {
		// Check which block is being written to
		int32_t block = writeSector / fatEraseSectors;
		if (block != writeBlock) {
			if (writeBlock > 0) {					// Write previously cached block to flash
				GPIOD->ODR |= GPIO_ODR_OD2;			// PD2: debug pin
				FlushCache();
				GPIOD->ODR &= ~GPIO_ODR_OD2;
			}

			// Load cache with current flash values
			writeBlock = block;
			const uint8_t* readAddress = flashAddress + (block * fatEraseSectors * fatSectorSize);
			memcpy(writeBlockCache, readAddress, fatEraseSectors * fatSectorSize);
		}

		// write cache is now valid - copy newly changed values into it
		uint32_t byteOffset = (writeSector - (block * fatEraseSectors)) * fatSectorSize;		// Offset within currently block
		memcpy(&(writeBlockCache[byteOffset]), readBuff, fatSectorSize * sectorCount);
		writeCacheDirty = true;
	}

	cacheUpdated = SysTickVal;
}


void FatTools::CheckCache()
{
	// If there are dirty buffers and sufficient time has elapsed since the cache updated flag was been set flush the cache to Flash
	if ((dirtyCacheBlocks || writeCacheDirty) && cacheUpdated > 0 && ((int32_t)SysTickVal - (int32_t)cacheUpdated) > 100)	{

		// Update the sample list to check if any meaningful data has changed (ignores Windows disk spam, assuming this occurs in the header cache)
		bool sampleChanged = false;
		if (dirtyCacheBlocks) {
			sampleChanged = UpdateSampleList();
		}

		if (sampleChanged || writeCacheDirty) {
			GPIOC->ODR |= GPIO_ODR_OD11;			// PC11: debug pin

			usb.PauseEndpoint(usb.msc);				// Sends NAKs from the msc endpoint whilst the Flash device is unavailable
			FlushCache();
			usb.ResumeEndpoint(usb.msc);

			GPIOC->ODR &= ~GPIO_ODR_OD11;
		}
		cacheUpdated = 0;

		GPIOB->ODR &= ~GPIO_ODR_OD14;				// PB14: Red LED nucleo
		busy = false;								// Current batch of writes has completed - release sample memory
	}
}


uint8_t FatTools::FlushCache()
{
	GPIOB->ODR |= GPIO_ODR_OD14;					// PB14: Red LED nucleo
	busy = true;									// Will be reset once CheckCache has confirmed that sufficient time has elapsed since last write

	// Writes any dirty data in the header cache to Flash
	uint8_t blockPos = 0;
	uint8_t count = 0;
	while (dirtyCacheBlocks != 0) {
		if (dirtyCacheBlocks & (1 << blockPos)) {
			uint32_t byteOffset = blockPos * fatEraseSectors * fatSectorSize;
			if (extFlash.WriteData(byteOffset, (uint32_t*)&(headerCache[byteOffset]), 1024, true)) {
				++count;
			}
			dirtyCacheBlocks &= ~(1 << blockPos);
		}
		++blockPos;
	}

	// Write current working block to flash
	if (writeCacheDirty && writeBlock > 0) {
		uint32_t writeAddress = writeBlock * fatEraseSectors * fatSectorSize;
		if (extFlash.WriteData(writeAddress, (uint32_t*)writeBlockCache, (fatEraseSectors * fatSectorSize) / 4, true)) {
			++count;
		}
		writeCacheDirty = false;			// Indicates that write cache is clean
	}
	return count;
}



bool FatTools::GetSampleInfo(SampleInfo* sample)
{
	// populate the sample object with sample rate, number of channels etc
	// Parsing the .wav format is a pain because the header is split into a variable number of chunks and sections are not word aligned

	const uint8_t* wavHeader = GetClusterAddr(sample->cluster);

	// Check validity
	if (*(uint32_t*)wavHeader != 0x46464952) {					// wav file should start with letters 'RIFF'
		return false;
	}

	// Jump through chunks looking for 'fmt' chunk
	uint32_t pos = 12;				// First chunk ID at 12 byte (4 word) offset
	while (*(uint32_t*)&(wavHeader[pos]) != 0x20746D66) {		// Look for string 'fmt '
		pos += (8 + *(uint32_t*)&(wavHeader[pos + 4]));			// Each chunk title is followed by the size of that chunk which can be used to locate the next one
		if  (pos > 100) {
			return false;
		}
	}

	sample->sampleRate = *(uint32_t*)&(wavHeader[pos + 12]);
	sample->channels = *(uint16_t*)&(wavHeader[pos + 10]);
	sample->bitDepth = *(uint16_t*)&(wavHeader[pos + 22]);

	// Navigate forward to find the start of the data area
	while (*(uint32_t*)&(wavHeader[pos]) != 0x61746164) {		// Look for string 'data'
		pos += (8 + *(uint32_t*)&(wavHeader[pos + 4]));
		if (pos > 200) {
			return false;
		}
	}

	sample->dataAddr = &(wavHeader[pos + 8]);
	return true;
}


bool FatTools::UpdateSampleList()
{
	// Updates list of samples from FAT root directory
	FATFileInfo* fatInfo;
	fatInfo = (FATFileInfo*)(headerCache + fatFs.dirbase * fatSectorSize);

	uint32_t pos = 0;
	bool changed = false;

	while (fatInfo->name[0] != 0) {
		// Check not LFN, not deleted, not directory, extension = WAV
		if (fatInfo->attr != 0xF && fatInfo->name[0] != 0xE5 && (fatInfo->attr & AM_DIR) == 0 && strncmp(&(fatInfo->name[8]), "WAV", 3) == 0) {
			SampleInfo* sample = &(sampleInfo[pos++]);

			// Check if any fields have changed
			if (sample->cluster != fatInfo->firstClusterLow || sample->size != fatInfo->fileSize || strncmp(sample->name, fatInfo->name, 11) != 0) {
				changed = true;
				strncpy(sample->name, fatInfo->name, 11);
				sample->cluster = fatInfo->firstClusterLow;
				sample->size = fatInfo->fileSize;

				sample->valid = GetSampleInfo(sample);
			}
		}
		fatInfo++;
	}

	// Blank next sample (if exists) to show end of list
	SampleInfo* sample = &(sampleInfo[pos++]);
	sample->name[0] = 0;

	return changed;
}


const uint8_t* FatTools::GetClusterAddr(uint32_t cluster)
{
	// Byte offset of the cluster start (note cluster numbers start at 2)
	uint32_t offsetByte = (fatFs.database * fatSectorSize) + (fatClusterSize * (cluster - 2));

	// Check if cluster is in cache or not
	if (offsetByte < fatCacheSectors * fatSectorSize) {			// In cache
		return headerCache + offsetByte;
	} else {
		return flashAddress + offsetByte;						// in memory mapped flash data
	}
}


void FillLfn(const char* desc, uint8_t* lfn)
{
	// Populate the long file name entry in the directory with the supplied string, converting into pseudo-unicode and splitting as required
	uint32_t idx = 1;

	while (*desc != '\0') {
		lfn[idx] = *desc++;
		idx += 2;
		if (idx == 11) idx = 14;		// name is split up so jump to next position
		if (idx == 26) idx = 28;
	}
}



void FatTools::MakeDummyFiles()
{
	// create block of memory to hold three directory entries holding the 'System Volum Information' folder
	uint8_t dirEntries[96] = {};
	FATLongFilename* dir = (FATLongFilename*)dirEntries;
	dir->attr = 0xF;								// LFN attribute
	dir->order = 0x42;								// 0x40 indicates first entry, 2 is second item in LFN
	FillLfn(" Information", (uint8_t*)dir);
	dir->checksum = 0x72;							// Checksum derived from short file name

	dir++;
	dir->attr = 0xF;
	dir->order = 0x1;								// 1 = first part of LFN
	FillLfn("System Volume", (uint8_t*)dir);
	dir->checksum = 0x72;

	FATFileInfo* sfn = (FATFileInfo*)&(dirEntries[64]);
	strncpy(sfn->name, "SYSTEM~1   ", 11);
	sfn->attr = (AM_HID | AM_SYS | AM_DIR);
	sfn->firstClusterLow = 2;						// Set cluster to 2 which is first available

	// Write to FAT header cache
	uint8_t* writeAddress = &(headerCache[fatFs.dirbase * fatSectorSize]);
	memcpy(writeAddress, dirEntries, 96);

	// Create cluster chain entry
	uint16_t* clusterChain = (uint16_t*)(fatTools.headerCache + (fatTools.fatFs.fatbase * fatSectorSize));
	clusterChain[2] = 0xFFFF;

	// Blank out directory in cluster 2
	memset(headerCache + (fatFs.database * fatSectorSize), 0, fatClusterSize);

}


void FatTools::PrintDirInfo(uint32_t cluster)
{
	// Output a detailed analysis of FAT directory structure
	FATFileInfo* fatInfo;
	if (cluster == 0) {
		printf("\r\n  Attrib Cluster  Bytes    Created   Accessed Name          Clusters\r\n"
				   "  ------------------------------------------------------------------\r\n");
		fatInfo = (FATFileInfo*)(headerCache + fatFs.dirbase * fatSectorSize);
	} else {
		fatInfo = (FATFileInfo*)GetClusterAddr(cluster);
	}

	if (fatInfo->fileSize == 0xFFFFFFFF) {					// Handle corrupt subdirectory (where flash has not been initialised
		return;
	}

	while (fatInfo->name[0] != 0) {
		if (fatInfo->attr == 0xF) {							// Long file name
			FATLongFilename* lfn = (FATLongFilename*)fatInfo;
			printf("%c LFN %2i                                      %-14s [0x%02x]\r\n",
					(cluster == 0 ? ' ' : '>'),
					lfn->order & (~0x40),
					GetFileName(fatInfo).c_str(),
					lfn->checksum);
		} else {
			printf("%c %s %8i %6lu %10s %10s %-14s",
					(cluster == 0 ? ' ' : '>'),
					(fatInfo->name[0] == 0xE5 ? "*Del*" : GetAttributes(fatInfo).c_str()),
					fatInfo->firstClusterLow,
					fatInfo->fileSize,
					FileDate(fatInfo->createDate).c_str(),
					FileDate(fatInfo->accessedDate).c_str(),
					GetFileName(fatInfo).c_str());

			// Print cluster chain
			if (fatInfo->name[0] != 0xE5 && fatInfo->fileSize > fatClusterSize) {

				bool seq = false;					// used to check for sequential blocks

				uint32_t cluster = fatInfo->firstClusterLow;
				uint16_t* clusterChain = (uint16_t*)(headerCache + (fatFs.fatbase * fatSectorSize));
				printf("%lu", cluster);

				while (clusterChain[cluster] != 0xFFFF) {
					if (clusterChain[cluster] == cluster + 1) {
						if (!seq) {
							printf("-");
							seq = true;
						}
					} else {
						seq = false;
						printf("%i, ", clusterChain[cluster]);
					}
					cluster = clusterChain[cluster];
				}
				if (seq) {
					printf("%lu", cluster);
				}
			}
			printf("\r\n");
		}

		// Recursively call function to print sub directory details (ignoring directories '.' and '..' which hold current and parent directory clusters
		if ((fatInfo->attr & AM_DIR) && (fatInfo->name[0] != '.')) {
			PrintDirInfo(fatInfo->firstClusterLow);
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


/*
 Time Format. A FAT directory entry time stamp is a 16-bit field that has a granularity of 2 seconds. Here is the format (bit 0 is the LSB of the 16-bit word, bit 15 is the MSB of the 16-bit word).

 Bits 0–4: 2-second count, valid value range 0–29 inclusive (0 – 58 seconds).
 Bits 5–10: Minutes, valid value range 0–59 inclusive.
 Bits 11–15: Hours, valid value range 0–23 inclusive.
 */

std::string FatTools::GetFileName(FATFileInfo* fi)
{
	char cs[14];
	std::string s;
	if (fi->attr == 0xF) {									// NB - unicode characters not properly handled - just assuming ASCII char in lower byte
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


void FatTools::InvalidateFatFSCache()
{
	// Clear the cache window in the fatFS object so that new writes will be correctly read
	fatFs.winsect = ~0;
}


// Uses FatFS to get a recursive directory listing (just shows paths and files)
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
