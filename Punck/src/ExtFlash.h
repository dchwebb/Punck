#pragma once

#include "initialisation.h"


/* FatFS Structure on 128 MBit Flash device:

Sector = 512 bytes
Cluster = 4 * 512 bytes = 2048 bytes
16 MBytes on Flash = 31,250 Sectors (7812.5 Clusters - data area is 7802 clusters after 10 clusters used for headers)

Bytes			Description
---------------------------
    0 -   511	Boot Sector (AKA Reserved): 1 sector
  512 - 16383	FAT (holds cluster linked list): 31 sectors - 7812 entries each 16 bit
16384 - 20479	Root Directory: 8 sectors - 128 root directory entries at 32 bytes each (32 * 128 = 4096)
20480 - 		Data section: 7803 clusters = 31,212 sectors

Cluster 2: 20480, Cluster 3: 22528, Cluster 4: 24576, Cluster 5: 26624, Cluster 6: 28672

*/

extern const uint32_t* flashAddress;
static constexpr uint32_t flashSectorSize = 512;		// Sector size used by FAT
static constexpr uint32_t flashClusterSize = 2048;		// Cluster size used by FAT (ie block size in data section)
static constexpr uint32_t flashEraseSectors = 8;		// Number of sectors in an erase block (4096 bytes)
static constexpr uint32_t flashSectorCount = 31250;		// 31250 sectors of 512 bytes = 16 MBytes
static constexpr uint32_t flashCacheSize = 72;			// Header: 1 sector Boot sector; 31 sectors FAT; 8 sectors Root Directory + extra for testing



extern uint8_t fatCache[flashSectorSize * flashCacheSize];


class ExtFlash {
public:
	enum qspiRegister : uint8_t {pageProgram = 0x02, quadPageProgram = 0x32, readData = 0x03, fastRead = 0x6B, fastReadIO = 0xEB, writeEnable = 0x06,
		readStatusReg1 = 0x05, readStatusReg2 = 0x35, readStatusReg3 = 0x15, writeStatusReg2 = 0x31,
		sectorErase = 0x20};

	void Init();
	void MemoryMapped();
	uint8_t ReadStatus(qspiRegister r);
	void WriteEnable();
	bool WriteData(uint32_t address, uint32_t* data, uint32_t words, bool checkErase = false);
	void BlockErase(uint32_t address);
	uint8_t ReadData(uint32_t address);
	uint32_t FastRead(uint32_t address);
	void CheckBusy();
	void InvalidateFATCache();

	bool memMapMode = false;
private:


};


extern ExtFlash extFlash;
