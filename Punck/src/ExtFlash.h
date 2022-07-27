#pragma once

#include "initialisation.h"

extern const uint8_t* flashAddress;

class ExtFlash {
public:
	enum qspiRegister : uint8_t {pageProgram = 0x02, quadPageProgram = 0x32, readData = 0x03, fastRead = 0x6B, fastReadIO = 0xEB, writeEnable = 0x06,
		readStatusReg1 = 0x05, readStatusReg2 = 0x35, readStatusReg3 = 0x15, writeStatusReg2 = 0x31,
		sectorErase = 0x20, chipErase = 0xC7};

	void Init();
	void MemoryMapped();
	void MemMappedOff();
	uint8_t ReadStatus(qspiRegister r);
	void WriteEnable();
	bool WriteData(uint32_t address, const uint32_t* data, uint32_t words, bool checkErase = false);
	void BlockErase(uint32_t address);
	void FullErase();
	uint8_t ReadData(uint32_t address);
	uint32_t FastRead(uint32_t address);
	void CheckBusy();
	void InvalidateFATCache();

	bool memMapMode = false;
private:


};


extern ExtFlash extFlash;
