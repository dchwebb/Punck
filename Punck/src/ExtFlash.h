#pragma once

#include "initialisation.h"

extern const uint8_t* flashAddress;

class ExtFlash {
public:
	enum qspiRegister : uint8_t {pageProgram = 0x02, quadPageProgram = 0x32, readData = 0x03, fastRead = 0x6B, fastReadIO = 0xEB, writeEnable = 0x06,
		readStatusReg1 = 0x05, readStatusReg2 = 0x35, readStatusReg3 = 0x15, writeStatusReg2 = 0x31,
		sectorErase = 0x20, chipErase = 0xC7, manufacturerID = 0x90, enableReset = 0x66, resetDevice = 0x99};

	void Init();
	uint8_t ReadStatus(qspiRegister r);
	void MemoryMapped();
	bool WriteData(uint32_t address, const uint32_t* data, uint32_t words);
	void BlockErase(const uint32_t address);
	void FullErase();
	uint8_t ReadData(const uint32_t address);
	uint32_t FastRead(const uint32_t address);
	uint16_t GetID();

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
