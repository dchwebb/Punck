#include "ExtFlash.h"
#include "FatTools.h"
#include <cstring>


ExtFlash extFlash;											// Singleton external flash handler

void ExtFlash::Init(bool hwInit)
{
	if (hwInit) {
		InitQSPI();											// Initialise hardware
	} else {
		SCB_InvalidateDCache_by_Addr((uint32_t*)flashAddress, fatSectorCount * fatSectorSize);		// Ensure cache is refreshed
	}

	Reset();
	uint32_t id = GetID();
	if (id == 0 || id == 255) {								// Winbond 128MB 0x17EF
		flashCorrupt = true;
		return;
	}

	// Quad SPI mode enable
	const uint16_t statusReg2 = ReadStatus(readStatusReg2);
	if ((statusReg2 & 2) == 0) {			// QuadSPI not enabled
		WriteEnable();

		QUADSPI->CR |= QUADSPI_CR_EN;						// Enable QSPI
		QUADSPI->CCR = (QUADSPI_CCR_DMODE_0 |				// 00: No data; *01: Data on a single line; 10: Data on two lines; 11: Data on four lines
						QUADSPI_CCR_IMODE_0 |				// 00: No instruction; *01: Instruction on single line; 10: Two lines; 11: Four lines
						(writeStatusReg2 << QUADSPI_CCR_INSTRUCTION_Pos));
		QUADSPI->DR = 2;									// Set Quad Enable bit
		while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
		QUADSPI->CR &= ~QUADSPI_CR_EN;						// Disable QSPI
	}

	MemoryMapped();											// Switch to memory mapped mode
	fatTools.InitFatFS();									// Initialise FatFS
}



void ExtFlash::MemoryMapped()
{
	// Activate memory mapped mode
	if (!memMapMode) {
		//SCB_InvalidateDCache_by_Addr(flashAddress, 10000);// Ensure cache is refreshed after write or erase
		while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
		CheckBusy();										// Check chip is not still writing data

		QUADSPI->ABR = 0xFF;								// Use alternate bytes to pad the address with a dummy byte of 0xFF
		QUADSPI->CR |= QUADSPI_CR_EN;						// Enable QSPI

		QUADSPI->CCR = (QUADSPI_CCR_FMODE |					// 00: Indirect write; 01: Indirect read; 10: Automatic polling; *11: Memory-mapped
						QUADSPI_CCR_ADSIZE_1 |				// Address: 00: 8-bit ; 01: 16-bit; *10: 24-bit; 11: 32-bit
						QUADSPI_CCR_ADMODE |				// Address: 00: None; 01: One line; 10: Two lines; *11: Four lines
						QUADSPI_CCR_ABMODE |				// Alternate Bytes: 00: None; 01: One line; 10: Two lines; *11: Four lines
						QUADSPI_CCR_DMODE |					// Data: 00: None; 01: One line; 10: Two lines; *11: Four lines
						QUADSPI_CCR_IMODE_0 |				// Instruction: 00: None; *01: One line; 10: Two lines;
						(4 << QUADSPI_CCR_DCYC_Pos) |		// insert 4 dummy clock cycles
						(fastReadIO << QUADSPI_CCR_INSTRUCTION_Pos));

		while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
		memMapMode = true;
	}
}


void ExtFlash::MemMappedOff()
{
	if (memMapMode) {
		QUADSPI->CR &= ~QUADSPI_CR_EN;						// Disable QSPI
		QUADSPI->CCR = 0;
		while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
		memMapMode = false;
	}
}


uint16_t ExtFlash::ReadStatus(qspiRegister r)
{
	MemMappedOff();
	QUADSPI->DLR = dualFlashMode ? 1 : 0;					// Return 1 byte for each flash used
	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI

	QUADSPI->CCR = (QUADSPI_CCR_FMODE_0 |					// 00: Indirect write mode; 01: Indirect read mode; 10: Automatic polling mode; 11: Memory-mapped mode
					QUADSPI_CCR_DMODE_0 |					// Data: 00: None; *01: One line; 10: Two lines; 11: Four lines
					QUADSPI_CCR_IMODE_0 |					// Instruction: 00: None; *01: One line; 10: Two lines; 11: Four lines
					(r << QUADSPI_CCR_INSTRUCTION_Pos));	// Read status register

	while ((QUADSPI->SR & QUADSPI_SR_TCF) == 0) {};			// Wait until transfer complete
	const uint16_t ret = QUADSPI->DR;
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
	return ret;
}


uint32_t ExtFlash::GetID()
{
	// Return manufacturer and Device ID
	MemMappedOff();
	QUADSPI->DLR = dualFlashMode ? 3 : 1;					// Return 2 bytes for each flash used
	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI

	QUADSPI->CCR = (QUADSPI_CCR_FMODE_0 |					// 00: Indirect write mode; *01: Indirect read mode; 10: Automatic polling mode; 11: Memory-mapped mode
					QUADSPI_CCR_ADSIZE_1 |					// Address: 00: 8-bit ; 01: 16-bit; *10: 24-bit; 11: 32-bit
					QUADSPI_CCR_ADMODE_0 |					// Address: 00: None; *01: One line; 10: Two lines; 11: Four lines
					QUADSPI_CCR_DMODE_0 |					// Data: 00: None; *01: One line; 10: Two lines; 11: Four lines
					QUADSPI_CCR_IMODE_0 |					// Instruction: 00: None; *01: One line; 10: Two lines; 11: Four lines
					(manufacturerID << QUADSPI_CCR_INSTRUCTION_Pos));
	QUADSPI->AR = 0;										// Use address 0x000000

	while ((QUADSPI->SR & QUADSPI_SR_TCF) == 0) {};			// Wait until transfer complete
	const uint32_t ret = QUADSPI->DR;
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
	return ret;
}


void ExtFlash::Reset()
{
	MemMappedOff();
	CheckBusy();
	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI
	QUADSPI->CCR = (QUADSPI_CCR_IMODE_0 |					// 00: No instruction; 01: Instruction on single line; 10: Two lines; 11: Four lines
					(enableReset << QUADSPI_CCR_INSTRUCTION_Pos));
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};

	//CheckBusy();
	QUADSPI->CCR = (QUADSPI_CCR_IMODE_0 |					// 00: No instruction; 01: Instruction on single line; 10: Two lines; 11: Four lines
					(resetDevice << QUADSPI_CCR_INSTRUCTION_Pos));
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
}


void ExtFlash::WriteEnable()
{
	MemMappedOff();
	CheckBusy();
	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI
	QUADSPI->CCR = (QUADSPI_CCR_IMODE_0 |					// 00: No instruction; 01: Instruction on single line; 10: Two lines; 11: Four lines
					(writeEnable << QUADSPI_CCR_INSTRUCTION_Pos));
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
}


bool ExtFlash::WriteData(uint32_t address, const uint32_t* writeBuff, uint32_t words)
{
	// Writes data to Flash memory breaking the writes at page boundaries; optionally checks if an erase is required first
	bool eraseRequired = false;
	bool dataChanged = false;
	uint32_t* memAddr = (uint32_t*)(flashAddress + address);	// address is passed relative to zero - store the memory mapped address

	for (uint32_t i = 0; i < words; ++i) {
		if (memAddr[i] != writeBuff[i]) {
			dataChanged = true;
			if ((memAddr[i] & writeBuff[i]) != writeBuff[i]) {	// 'And' tests if any bits that need to be set are currently zero - ie needing an erase
				eraseRequired = true;
				break;
			}
		}
	}
	if (!dataChanged) {										// No difference between Flash contents and write data
		return false;
	}

	if (eraseRequired) {
		BlockErase(address & ~(fatEraseSectors - 1));		// Force address to 4096 byte (8192 in dual flash mode) boundary
	}

	uint32_t remainingWords = words;
	do {
		WriteEnable();

		// Can write 256 bytes (64 words) at a time, and must be aligned to page boundaries (256 bytes)
		const uint32_t startPage = (address >> 8);
		const uint32_t endPage = (((address + (remainingWords * 4)) - 1) >> 8);


		uint32_t writeSize = remainingWords * 4;			// Size of current write in bytes
		if (endPage != startPage) {
			writeSize = ((startPage + 1) << 8) - address;	// When crossing pages only write up to the 256 byte boundary
		}

		QUADSPI->DLR = writeSize - 1;						// number of bytes - 1 to transmit
		QUADSPI->CR |= (3 << QUADSPI_CR_FTHRES_Pos);		// Set the threshold to 4 bytes
		QUADSPI->CCR = 0;

		QUADSPI->CR |= QUADSPI_CR_EN;						// Enable QSPI

		QUADSPI->CCR = (QUADSPI_CCR_ADSIZE_1 |				// Address: 00: 8-bit ; 01: 16-bit; *10: 24-bit; 11: 32-bit
						QUADSPI_CCR_ADMODE_0 |				// Address: 00: None; *01: One line; 10: Two lines; 11: Four lines
						QUADSPI_CCR_DMODE |					// Data: 00: None; 01: One line; 10: Two lines; *11: Four lines
						QUADSPI_CCR_IMODE_0 |				// Instruction: 00: None; *01: One line; 10: Two lines; 11: Four lines
						(quadPageProgram << QUADSPI_CCR_INSTRUCTION_Pos));
		QUADSPI->AR = address;
		for (uint8_t i = 0; i < (writeSize / 4); ++i) {
			QUADSPI->DR = *writeBuff++;
			while ((QUADSPI->SR & QUADSPI_SR_FTF) == 0) {};
		}

		remainingWords -= (writeSize / 4);
		address += writeSize;

		while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
		QUADSPI->CR &= ~QUADSPI_CR_EN;						// Disable QSPI
	} while (remainingWords > 0);

	SCB_InvalidateDCache_by_Addr(memAddr, words * 4);		// Ensure cache is refreshed after write or erase

	MemoryMapped();											// Switch back to memory mapped mode
	return true;
}


void ExtFlash::BlockErase(const uint32_t address)
{
	WriteEnable();
	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI

	QUADSPI->CCR = (QUADSPI_CCR_ADSIZE_1 |					// Address: 00: 8-bit ; 01: 16-bit; *10: 24-bit; 11: 32-bit
					QUADSPI_CCR_ADMODE_0 |					// Address: 00: None; *01: One line; 10: Two lines; 11: Four lines
					QUADSPI_CCR_IMODE_0 |					// Instruction: 00: None; *01: One line; 10: Two lines; 11: Four lines
					(sectorErase << QUADSPI_CCR_INSTRUCTION_Pos));
	QUADSPI->AR = address;
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
}


void ExtFlash::FullErase()
{
	WriteEnable();
	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI

	QUADSPI->CCR = (QUADSPI_CCR_IMODE_0 |					// Instruction: 00: None; *01: One line; 10: Two lines; 11: Four lines
					(chipErase << QUADSPI_CCR_INSTRUCTION_Pos));
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
	MemoryMapped();
}


uint8_t ExtFlash::ReadData(const uint32_t address)
{
	// Read a single byte of data
	QUADSPI->DLR = 0;										// Return 1 byte
	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI

	QUADSPI->CCR = (QUADSPI_CCR_FMODE_0 |					// 00: Indirect write mode; *01: Indirect read mode; 10: Automatic polling mode; 11: Memory-mapped mode
					QUADSPI_CCR_ADSIZE_1 |					// Address: 00: 8-bit ; 01: 16-bit; *10: 24-bit; 11: 32-bit
					QUADSPI_CCR_ADMODE_0 |					// Address: 00: None; *01: One line; 10: Two lines; 11: Four lines
					QUADSPI_CCR_DMODE_0 |					// Data: 00: None; *01: One line; 10: Two lines; 11: Four lines
					QUADSPI_CCR_IMODE_0 |					// Instruction: 00: None; *01: One line; 10: Two lines; 11: Four lines
					(readData << QUADSPI_CCR_INSTRUCTION_Pos));
	QUADSPI->AR = address;

	while ((QUADSPI->SR & QUADSPI_SR_TCF) == 0) {};			// Wait until transfer complete
	const uint8_t ret = QUADSPI->DR;
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
	return ret;
}


uint32_t ExtFlash::FastRead(const uint32_t address)
{
	const bool memoryMapped = memMapMode;
	MemMappedOff();
	QUADSPI->DLR = 0x3;										// Return 4 bytes
	QUADSPI->ABR = 0xFF;									// Use alternate bytes to pad the address with a dummy byte of 0xFF
	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI

	QUADSPI->CCR = (QUADSPI_CCR_FMODE_0 |					// 00: Indirect write mode; *01: Indirect read mode; 10: Automatic polling mode; 11: Memory-mapped mode
					QUADSPI_CCR_ADSIZE_1 |					// Address: 00: 8-bit ; 01: 16-bit; *10: 24-bit; 11: 32-bit
					QUADSPI_CCR_ADMODE |					// Address: 00: None; 01: One line; 10: Two lines; *11: Four lines
					QUADSPI_CCR_ABMODE |					// Alternate Bytes: 00: None; 01: One line; 10: Two lines; *11: Four lines
					QUADSPI_CCR_DMODE |						// Data: 00: None; 01: One line; 10: Two lines; *11: Four lines
					QUADSPI_CCR_IMODE_0 |					// Instruction: 00: None; *01: One line; 10: Two lines;
					(4 << QUADSPI_CCR_DCYC_Pos) |			// insert 8 dummy clock cycles
					(fastReadIO << QUADSPI_CCR_INSTRUCTION_Pos));
	QUADSPI->AR = address;									// Address needs to be four bytes with a dummy byte of 0xFF

	while ((QUADSPI->SR & QUADSPI_SR_TCF) == 0) {};			// Wait until transfer complete
	const uint32_t ret = QUADSPI->DR;
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI

	if (memoryMapped) {										// Switch back into memory mapped mode
		MemoryMapped();
	}
	return ret;
}


void ExtFlash::CheckBusy()
{
	// Use Automatic Polling mode to wait until Flash Busy flag is cleared
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
	QUADSPI->DLR = 0;										// Return 1 byte
	QUADSPI->PSMKR = 0b00000001;							// Mask on bit 1 (Busy)
	QUADSPI->PSMAR = 0b00000000;							// Match Busy = 0
	QUADSPI->PIR = 0x10;									// Polling interval in clock cycles
	QUADSPI->CR |= QUADSPI_CR_APMS;							// Set the 'auto-stop' bit to end transaction after a match.

	QUADSPI->CCR = (QUADSPI_CCR_FMODE_1 |					// 00: Indirect write; 01: Indirect read; *10: Automatic polling; 11: Memory-mapped
					QUADSPI_CCR_DMODE_0 |					// Data: 00: None; *01: One line; 10: Two lines; 11: Four lines
					QUADSPI_CCR_IMODE_0);					// Instruction: 00: None; *01: One line; 10: Two lines; 11: Four lines

	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI

	QUADSPI->CCR |= (readStatusReg1 << QUADSPI_CCR_INSTRUCTION_Pos);

	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->FCR |= QUADSPI_FCR_CSMF;						// Acknowledge status match flag
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
}



