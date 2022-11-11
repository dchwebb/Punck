#pragma once

#include "initialisation.h"

static uint8_t* const flashAddress = reinterpret_cast<uint8_t*>(0x90000000);			// Location that Flash storage will be accessed in memory mapped mode
static constexpr bool dualFlashMode = true;

class ExtFlash {
public:
	enum qspiRegister : uint8_t {pageProgram = 0x02, quadPageProgram = 0x32, readData = 0x03, fastRead = 0x6B, fastReadIO = 0xEB, writeEnable = 0x06,
		readStatusReg1 = 0x05, readStatusReg2 = 0x35, readStatusReg3 = 0x15, writeStatusReg2 = 0x31,
		sectorErase = 0x20, chipErase = 0xC7, manufacturerID = 0x90, enableReset = 0x66, resetDevice = 0x99};

	void Init(bool hwInit = true);
	uint16_t ReadStatus(qspiRegister r);
	void MemoryMapped();
	bool WriteData(uint32_t address, const uint32_t* data, uint32_t words);
	void BlockErase(const uint32_t address);
	void FullErase();
	uint8_t ReadData(const uint32_t address);
	uint32_t FastRead(const uint32_t address);
	uint32_t GetID();

	bool memMapMode = false;
	bool flashCorrupt = false;
private:
	void Reset();
	void CheckBusy();
	void MemMappedOff();
	void WriteEnable();
	void InvalidateFATCache();

};


extern ExtFlash extFlash;
