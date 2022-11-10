#pragma once

#include <string>
#include <array>
#include "ff.h"


/* FAT16 Structure on 128 MBit Flash device:

Sector = 512 bytes
Cluster = 4 * 512 bytes = 2048 bytes
Block = 4096 bytes (minimum erase size)

16 MBytes on Flash = 31,250 Sectors (7812.5 Clusters - data area is 7802 clusters after 10 clusters used for headers)

Blocks    Bytes			Description
------------------------------------
0           0 -   511	Boot Sector (AKA Reserved): 1 sector
0-3       512 - 16383	FAT (holds cluster linked list): 31 sectors - 7812 entries each 16 bit
4       16384 - 20479	Root Directory: 8 sectors - 128 root directory entries at 32 bytes each (32 * 128 = 4096)
5-      20480 - 		Data section: 7803 clusters = 31,212 sectors

Cluster 2: 20480, Cluster 3: 22528, Cluster 4: 24576, Cluster 5: 26624, Cluster 6: 28672
*/

static constexpr uint32_t fatSectorSize = 512;			// Sector size used by FAT
static constexpr uint32_t fatSectorCount = 31250;		// 31250 sectors of 512 bytes = 16 MBytes
static constexpr uint32_t fatClusterSize = 2048;		// Cluster size used by FAT (ie block size in data section)
static constexpr uint32_t fatEraseSectors = 8;			// Number of sectors in an erase block (4096 bytes)
static constexpr uint32_t fatHeaderSectors = 40;		// Number of sectors in all header regions [1 sector Boot sector; 31 sectors FAT; 8 sectors Root Directory]
static constexpr uint32_t fatCacheSectors = 72;			// Header + extra for testing


// Struct to hold regular 32 byte directory entries (SFN)
struct FATFileInfo {
	char name[11];						// Short name: If name[0] == 0xE5 directory entry is free (0x00 also means free and rest of directory is free)
	uint8_t attr;						// READ_ONLY 0x01; HIDDEN 0x02; SYSTEM 0x04; VOLUME_ID 0x08; DIRECTORY 0x10; ARCHIVE 0x20; LONG_NAME 0xF
	uint8_t NTRes;						// Reserved for use by Windows NT
	uint8_t createTimeTenth;			// File creation time in count of tenths of a second
	uint16_t createTime;				// Time file was created
	uint16_t createDate;				// Date file was created
	uint16_t accessedDate;				// Last access date
	uint16_t firstClusterHigh;			// High word of first cluster number (always 0 for a FAT12 or FAT16 volume)
	uint16_t writeTime;					// Time of last write. Note that file creation is considered a write
	uint16_t writeDate;					// Date of last write
	uint16_t firstClusterLow;			// Low word of this entry’s first cluster number
	uint32_t fileSize;					// File size in bytes
};

// Long File Name (LFN) directory entries
struct FATLongFilename {
	uint8_t order;						// Order in sequence of LFN entries associated with the short dir entry at the end of the LFN set. If masked with 0x40 entry is the last long dir entry
	char name1[10];						// Characters 1-5 of the long-name sub-component in this dir entry
	uint8_t attr;						// Attribute - must be 0xF
	uint8_t Type;						// If zero, indicates a directory entry that is a sub-component of a long name
	uint8_t checksum;					// Checksum of name in the short dir entry at the end of the long dir set
	char name2[12];						// Characters 6-11 of the long-name sub-component in this dir entry
	uint16_t firstClusterLow;			// Must be ZERO
	char name3[4];						// Characters 12-13 of the long-name sub-component in this dir entry
};



// Class provides interface with FatFS library and some low level helpers not provided with the library
class FatTools
{
	friend class CDCHandler;
public:
	bool busy = false;
	bool noFileSystem = true;
	uint16_t* clusterChain;				// Pointer to beginning of cluster chain (AKA FAT)
	FATFileInfo* rootDirectory;			// Pointer to start of FAT directory listing

	bool InitFatFS();
	void Read(uint8_t* writeAddress, const uint32_t readSector, const uint32_t sectorCount);
	const uint8_t* GetSectorAddr(const uint32_t sector, const uint8_t* buffer, const uint32_t bufferSize);
	const uint8_t* GetClusterAddr(const uint32_t cluster, const bool ignoreCache = false);
	void Write(const uint8_t* readBuff, const uint32_t writeSector, const uint32_t sectorCount);
	void PrintDirInfo(uint32_t cluster = 0);
	void PrintFiles(char* path);
	void CheckCache();
	uint8_t FlushCache();
	void InvalidateFatFSCache();
	bool Format();
private:
	FATFS fatFs;						// File system object for RAM disk logical drive
	const char fatPath[4] = "0:/";		// Logical drive path for FAT File system

	// Create cache for header part of FAT [Header consists of 1 sector boot sector; 31 sectors FAT; 8 sectors Root Directory + extra]
	uint32_t cacheUpdated = 0;			// Store the systick time the cache was last updated so separate timer can periodically clean up cache
	uint8_t headerCache[fatSectorSize * fatCacheSectors];	//
	uint32_t dirtyCacheBlocks = 0;		// Bit array containing dirty blocks in header cache

	// Initialise Write Cache - this is used to cache write data into blocks for safe erasing when overwriting existing data
	uint8_t writeBlockCache[fatSectorSize * fatEraseSectors];
	int32_t writeBlock = -1;			// Keep track of which block is currently held in the write cache
	bool writeCacheDirty = false;		// Indicates whether the data in the write cache has changes

	std::string GetFileName(const FATFileInfo* lfn);
	std::string GetAttributes(const FATFileInfo* fi);
	std::string FileDate(const uint16_t date);
	void MakeDummyFiles();
	void LFNDirEntries(uint8_t* address, const char* sfn, const char* lfn1, const char* lfn2, const uint8_t checksum, const uint8_t attributes, const uint16_t cluster, const uint32_t size);

};


// Create struct to hold wav file header
struct WavFile {
	char id[4];					// 'RIFF'
	uint32_t chunkSize;			// Size of the entire file in bytes minus 8 bytes
	char format[4];        		// 'WAVE'
	char Subchunk1ID[4];   		// 'fmt '
	uint32_t subChunk1Size;		// Sub chunk 1 size (ie 16) -effectively  header size
	uint16_t audioFormat;  		// 1 = PCM format
	uint16_t numChannels;  		// Number of channels = 1
	uint32_t sampleRate;   		// sample rate
	uint32_t byteRate;     		// byte rate (ie 1 channels, 8 bit = 1 bytes per sample)
	uint16_t blockAlign;   		// Block align = 4
	uint16_t bitsPerSample;		// Bits per sample = 8
	char subChunk2ID[4];   		// 'data'
	uint32_t subChunk2Size;		// subchunk 2 = number of bytes in the data. NumSamples * NumChannels * BitsPerSample/8
};

extern FatTools fatTools;



