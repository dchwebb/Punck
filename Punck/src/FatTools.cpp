#include "FatTools.h"
#include "ExtFlash.h"
#include "VoiceManager.h"
#include <cstring>
#include "usb.h"

FatTools fatTools;


bool FatTools::InitFatFS()
{
	if (extFlash.flashCorrupt) {
		return false;
	}

	// Set up cache area for header data
	memcpy(headerCache, flashAddress, fatSectorSize * fatCacheSectors);

	const FRESULT res = f_mount(&fatFs, fatPath, 1) ;		// Register the file system object to the FatFs module
	if (res == FR_NO_FILESYSTEM) {
		return false;

	}
	noFileSystem = false;

	// Store the address of the cluster chain for speed in future lookups
	clusterChain = (uint16_t*)(headerCache + (fatFs.fatbase * fatSectorSize));
	clusterChain[1] |= 0x8000;								// Remove ClnShutBitMask where Windows records a badly shut down drive

	// Store pointer to start of root directoy
	rootDirectory = (FATFileInfo*)(headerCache + fatFs.dirbase * fatSectorSize);

	voiceManager.samples.UpdateSampleList();				// Updated list of samples on flash

	return true;
}


bool FatTools::Format()
{
	printf("Erasing flash ...\r\n");
	extFlash.FullErase();

	printf("Mounting File System ...\r\n");
	uint8_t fsWork[fatSectorSize];							// Work buffer for the f_mkfs()
	MKFS_PARM parms;										// Create parameter struct
	parms.fmt = FM_FAT | FM_SFD;							// format as FAT12/16 using SFD (Supper Floppy Drive)
	parms.n_root = 128;										// Number of root directory entries (each uses 32 bytes of storage)
	parms.align = 0;										// Default initialise remaining values
	parms.au_size = 0;
	parms.n_fat = 0;

	f_mkfs(fatPath, &parms, fsWork, sizeof(fsWork));		// Make file system in header cache (via diskio.cpp helper functions)
	f_mount(&fatFs, fatPath, 1) ;							// Mount the file system (needed to set up FAT locations for next functions)

	printf("Creating system files ...\r\n");
	MakeDummyFiles();										// Create Windows index files to force them to be created in header cache

	printf("Flushing cache ...\r\n");
	FlushCache();

	if (InitFatFS()) {										// Mount FAT file system and initialise directory pointers
		printf("Successfully formatted drive\r\n");
		return true;
	} else {
		printf("Failed to format drive\r\n");
		return false;
	}
}


void FatTools::Read(uint8_t* writeAddress, const uint32_t readSector, const uint32_t sectorCount)
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


void FatTools::Write(const uint8_t* readBuff, const uint32_t writeSector, const uint32_t sectorCount)
{
	if (writeSector < fatCacheSectors) {
		// Update the bit array of dirty blocks [There are 8 x 512 byte sectors in a block (4096)]
		dirtyCacheBlocks |= (1 << (writeSector / fatEraseSectors));

		uint8_t* writeAddress = &(headerCache[writeSector * fatSectorSize]);
		memcpy(writeAddress, readBuff, fatSectorSize * sectorCount);
	} else {
		// Check which block is being written to
		const int32_t block = writeSector / fatEraseSectors;
		if (block != writeBlock) {
			if (writeBlock > 0) {					// Write previously cached block to flash
				FlushCache();
			}

			// Load cache with current flash values
			writeBlock = block;
			const uint8_t* readAddress = flashAddress + (block * fatEraseSectors * fatSectorSize);
			memcpy(writeBlockCache, readAddress, fatEraseSectors * fatSectorSize);
		}

		// write cache is now valid - copy newly changed values into it
		const uint32_t byteOffset = (writeSector - (block * fatEraseSectors)) * fatSectorSize;		// Offset within currently block
		memcpy(&(writeBlockCache[byteOffset]), readBuff, fatSectorSize * sectorCount);
		writeCacheDirty = true;
	}

	cacheUpdated = SysTickVal;
}


void FatTools::CheckCache()
{
	// If dirty buffers and sufficient time has elapsed since cache updated flush the cache to Flash
	if ((dirtyCacheBlocks || writeCacheDirty) && cacheUpdated > 0 && ((int32_t)SysTickVal - (int32_t)cacheUpdated) > 100)	{

		// Update the sample list to check if any meaningful data has changed (ignores Windows disk spam, assuming this occurs in the header cache)
		bool sampleChanged = false;
		if (dirtyCacheBlocks) {
			sampleChanged = voiceManager.samples.UpdateSampleList();
		}

		if (sampleChanged || writeCacheDirty) {
			usb.PauseEndpoint(usb.msc);				// Sends NAKs from the msc endpoint whilst the Flash device is unavailable
			FlushCache();
			usb.ResumeEndpoint(usb.msc);
		}
		cacheUpdated = 0;

		busy = false;								// Current batch of writes has completed - release sample memory
	}
}


uint8_t FatTools::FlushCache()
{
	busy = true;									// Will be reset once CheckCache has confirmed that sufficient time has elapsed since last write

	// Writes any dirty data in the header cache to Flash
	uint8_t blockPos = 0;
	uint8_t count = 0;
	while (dirtyCacheBlocks != 0) {
		if (dirtyCacheBlocks & (1 << blockPos)) {
			uint32_t byteOffset = blockPos * fatEraseSectors * fatSectorSize;
			if (extFlash.WriteData(byteOffset, (uint32_t*)&(headerCache[byteOffset]), 1024)) {
				++count;
			}
			dirtyCacheBlocks &= ~(1 << blockPos);
		}
		++blockPos;
	}

	// Write current working block to flash
	if (writeCacheDirty && writeBlock > 0) {
		uint32_t writeAddress = writeBlock * fatEraseSectors * fatSectorSize;
		if (extFlash.WriteData(writeAddress, (uint32_t*)writeBlockCache, (fatEraseSectors * fatSectorSize) / 4)) {
			++count;
		}
		writeCacheDirty = false;			// Indicates that write cache is clean
	}
	return count;
}






const uint8_t* FatTools::GetClusterAddr(const uint32_t cluster, const bool ignoreCache)
{
	// Byte offset of the cluster start (note cluster numbers start at 2)
	const uint32_t offsetByte = (fatFs.database * fatSectorSize) + (fatClusterSize * (cluster - 2));

	// Check if cluster is in cache or not
	if (offsetByte < fatCacheSectors * fatSectorSize && !ignoreCache) {			// In cache
		return headerCache + offsetByte;
	} else {
		return flashAddress + offsetByte;						// in memory mapped flash data
	}
}


const uint8_t* FatTools::GetSectorAddr(const uint32_t sector, const uint8_t* buffer, const uint32_t bufferSize)
{
	// If reading header data return cache address; if reading flash address and buffer passed trigger DMA transfer and return null pointer
	// FIXME - take into account write cache??
	if (sector < fatCacheSectors) {
		return &(headerCache[sector * fatSectorSize]);
	} else {
		const uint8_t* sectorAddress = flashAddress + (sector * fatSectorSize);
		if (buffer == nullptr) {
			return sectorAddress;
		} else {
			MDMATransfer(sectorAddress, buffer, bufferSize);
			return nullptr;
		}
	}
}


void FatTools::PrintDirInfo(const uint32_t cluster)
{
	// Output a detailed analysis of FAT directory structure
	if (noFileSystem) {
		printf("** No file System **\r\n");
		return;
	}

	const FATFileInfo* fatInfo;
	if (cluster == 0) {
		printf("\r\n  Attrib Cluster   Bytes    Created   Accessed Name          Clusters\r\n"
				   "  ------------------------------------------------------------------\r\n");
		fatInfo = rootDirectory;
	} else {
		fatInfo = (FATFileInfo*)GetClusterAddr(cluster);
	}

	if (fatInfo->fileSize == 0xFFFFFFFF && *(uint32_t*)fatInfo->name == 0xFFFFFFFF) {		// Handle corrupt subdirectory (where flash has not been initialised
		return;
	}

	while (fatInfo->name[0] != 0) {
		if (fatInfo->attr == 0xF) {							// Long file name
			const FATLongFilename* lfn = (FATLongFilename*)fatInfo;
			printf("%c LFN %2i                                       %-14s [0x%02x]\r\n",
					(cluster == 0 ? ' ' : '>'),
					lfn->order & (~0x40),
					GetFileName(fatInfo).c_str(),
					lfn->checksum);
		} else {
			printf("%c %s %8i %7lu %10s %10s %-14s",
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
				printf("%lu", cluster);

				while (clusterChain[cluster] != 0xFFFF) {
					if (clusterChain[cluster] == cluster + 1) {
						if (!seq) {
							printf("-");
							seq = true;
						}
					} else {
						seq = false;
						printf("%lu, %i", cluster, clusterChain[cluster]);
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


std::string FatTools::FileDate(const uint16_t date)
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

std::string FatTools::GetFileName(const FATFileInfo* fi)
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


std::string FatTools::GetAttributes(const FATFileInfo* fi)
{
	const char cs[6] = {(fi->attr & AM_RDO) ? 'R' : '-',
			(fi->attr & AM_HID) ? 'H' : '-',
			(fi->attr & AM_SYS) ? 'S' : '-',
			(fi->attr & AM_DIR) ? 'D' : '-',
			(fi->attr & AM_ARC) ? 'A' : '-', '\0'};
	const std::string s = std::string(cs);
	return s;
}


void FillLFN(const char* desc, uint8_t* lfn)
{
	// Populate the long file name entry in the directory with the supplied string, converting into pseudo-unicode and splitting as required
	uint32_t idx = 1;
	bool padding = false;				// Will be set to true when reached the end of the text and starting to pad with 0xFF

	while (idx < 32) {
		if (!padding) {
			if (*desc == '\0') {
				padding = true;
			}
			lfn[idx] = *desc++;
			idx += 2;
		} else {
			lfn[idx++] = 0xFF;
		}
		if (idx == 11) idx = 14;		// name is split up so jump to next position
		if (idx == 26) idx = 28;
	}
}


void FatTools::LFNDirEntries(uint8_t* writeAddress, const char* sfn, const char* lfn1, const char* lfn2, const uint8_t checksum, const uint8_t attributes, const uint16_t cluster, const uint32_t size)
{
	// Quick 'n' Dirty routine to generate 2 LFN entries and main file entry for Windows dummy files
	uint8_t dirEntries[96] = {};
	FATLongFilename* dir = (FATLongFilename*)dirEntries;
	dir->attr = 0xF;								// LFN attribute
	dir->order = 0x42;								// 0x40 indicates first entry, 2 is second item in LFN
	FillLFN(lfn1, (uint8_t*)dir);
	dir->checksum = checksum;						// Checksum derived from short file name

	++dir;
	dir->attr = 0xF;
	dir->order = 0x1;								// 1 = first part of LFN
	FillLFN(lfn2, (uint8_t*)dir);
	dir->checksum = checksum;

	FATFileInfo* ds = (FATFileInfo*)&(dirEntries[64]);
	strncpy(ds->name, sfn, 11);
	ds->attr = attributes;
	ds->firstClusterLow = cluster;					// Set cluster to 2 which is first available
	ds->fileSize = size;

	memcpy(writeAddress, dirEntries, 96);

	uint16_t* clusters = (uint16_t*)(headerCache + (fatFs.fatbase * fatSectorSize));
	clusters[cluster] = 0xFFFF;						// Create cluster chain entry (do not use clusterChain as not yet initialised)
}


void FatTools::MakeDummyFiles()
{
	// Kludgy code to create Windows spam folders/files to stop OS doing it where we don't want

	// create block of memory to hold three directory entries holding the 'System Volume Information' folder
	uint8_t* writeAddress = &(headerCache[fatFs.dirbase * fatSectorSize]);		// do not use rootDirectory member as not yet initialised
	LFNDirEntries(writeAddress, "SYSTEM~1   ", " Information", "System Volume", 0x72, (AM_HID | AM_SYS | AM_DIR), 2, 0);

	// Blank out directory in cluster 2
	uint8_t* cluster2Addr = headerCache + (fatFs.database * fatSectorSize);
	memset(cluster2Addr, 0, fatClusterSize);

	// Create '.' and '..' entries for 'System Volume Information' folder in cluster 2
	uint8_t dirEntries[64] = {};
	FATFileInfo* dir = (FATFileInfo*)dirEntries;
	strncpy(dir->name, ".          ", 11);
	dir->attr = AM_DIR;
	dir->firstClusterLow = 2;						// '.' path points to containing directory cluster

	++dir;
	strncpy(dir->name, "..         ", 11);
	dir->attr = AM_DIR;
	dir->firstClusterLow = 0;						// '..' path points to root
	memcpy(cluster2Addr, dirEntries, 64);

	// Create 'IndexerVolumeGuid' and 'WPSettings.dat' files
	LFNDirEntries(cluster2Addr + 64, "INDEXE~1   ", "Guid", "IndexerVolume", 0xff, AM_ARC, 3, 76);
	uint8_t* cluster3Addr = cluster2Addr + fatClusterSize;
	memcpy(cluster3Addr, "{DC903617-185C-4094-875D-7E83AE4C738E}", 76);		// Create file - no idea if the actual GUID matters but whatever

	LFNDirEntries(cluster2Addr + 160, "WPSETT~1DAT", "t", "WPSettings.da", 0xce, AM_ARC, 4, 12);
	uint32_t file[3] = {0x0000000c, 0x5e3739ac, 0x0b1690d4};				// This random file is just what it creates on my PC - undocumented
	memcpy(cluster3Addr + fatClusterSize, file, 12);

	// Mark clusters 2 and 3 cache as dirty
	uint32_t blockCount = fatFs.database / fatEraseSectors;
	dirtyCacheBlocks |= (1 << (blockCount / 1));
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
				const uint32_t i = strlen(path);
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
