#include "ExtFlash.h"

ExtFlash extFlash;

void ExtFlash::Init()
{
	InitQSPI();												// Initialise hardware

	// Write enable
	if ((ReadStatus(readStatusReg1) & 2) == 0) {
		QUADSPI->CCR = QUADSPI_CCR_IMODE_0;					// 00: No instruction; 01: Instruction on single line; 10: Two lines; 11: Four lines
		QUADSPI->CR |= QUADSPI_CR_EN;						// Enable QSPI
		QUADSPI->CCR |= (writeEnable << QUADSPI_CCR_INSTRUCTION_Pos);
		while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
		QUADSPI->CR &= ~QUADSPI_CR_EN;						// Disable QSPI
	}

	// Quad SPI mode enable
	if ((ReadStatus(readStatusReg2) & 2) == 0) {
		QUADSPI->CCR = (QUADSPI_CCR_DMODE_0 |				// 00: No data; 01: Data on a single line; 10: Data on two lines; 11: Data on four lines
						QUADSPI_CCR_IMODE_0);				// 00: No instruction; 01: Instruction on single line; 10: Two lines; 11: Four lines
		QUADSPI->CR |= QUADSPI_CR_EN;						// Enable QSPI
		QUADSPI->CCR |= (writeStatusReg2 << QUADSPI_CCR_INSTRUCTION_Pos);
		QUADSPI->DR = 2;									// Set Quad Enable bit
		while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
		QUADSPI->CR &= ~QUADSPI_CR_EN;						// Disable QSPI
	}
}


void ExtFlash::MemoryMapped()
{
	// Activate memory mapped mode
	QUADSPI->ABR = 0xFF;									// Use alternate bytes to pad the address with a dummy byte of 0xFF
	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI

	QUADSPI->CCR = (QUADSPI_CCR_FMODE |						// 00: Indirect write; 01: Indirect read; 10: Automatic polling; *11: Memory-mapped
					QUADSPI_CCR_ADSIZE_1 |					// Address: 00: 8-bit ; 01: 16-bit; *10: 24-bit; 11: 32-bit
					QUADSPI_CCR_ADMODE |					// Address: 00: None; 01: One line; 10: Two lines; *11: Four lines
					QUADSPI_CCR_ABMODE |					// Alternate Bytes: 00: None; 01: One line; 10: Two lines; *11: Four lines
					QUADSPI_CCR_DMODE |						// Data: 00: None; 01: One line; 10: Two lines; *11: Four lines
					QUADSPI_CCR_IMODE_0 |					// Instruction: 00: None; *01: One line; 10: Two lines;
					(4 << QUADSPI_CCR_DCYC_Pos) |			// insert 4 dummy clock cycles
					(fastReadIO << QUADSPI_CCR_INSTRUCTION_Pos));

	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI
}


uint8_t ExtFlash::ReadStatus(qspiRegister r)
{
	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI

	QUADSPI->CCR = (QUADSPI_CCR_FMODE_0 |					// 00: Indirect write mode; 01: Indirect read mode; 10: Automatic polling mode; 11: Memory-mapped mode
					 QUADSPI_CCR_DMODE_0 |					// Data: 00: None; *01: One line; 10: Two lines; 11: Four lines
					QUADSPI_CCR_IMODE_0 |					// Instruction: 00: None; *01: One line; 10: Two lines; 11: Four lines
					(r << QUADSPI_CCR_INSTRUCTION_Pos));	// Read status register

	while ((QUADSPI->SR & QUADSPI_SR_TCF) == 0) {};			// Wait until transfer complete
	uint8_t ret = QUADSPI->DR;
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
	return ret;
}


void ExtFlash::WriteData(uint32_t address, uint8_t data)
{
	QUADSPI->DLR = 0;										// 1 byte
	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI

	QUADSPI->CCR = (QUADSPI_CCR_ADSIZE_1 |					// Address: 00: 8-bit ; 01: 16-bit; *10: 24-bit; 11: 32-bit
					QUADSPI_CCR_ADMODE_0 |					// Address: 00: None; *01: One line; 10: Two lines; 11: Four lines
					QUADSPI_CCR_DMODE_0 |					// Data: 00: None; *01: One line; 10: Two lines; 11: Four lines
					QUADSPI_CCR_IMODE_0 |					// Instruction: 00: None; *01: One line; 10: Two lines; 11: Four lines
					(pageProgram << QUADSPI_CCR_INSTRUCTION_Pos));
	QUADSPI->AR = address;
	QUADSPI->DR = data;
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
}


uint8_t ExtFlash::ReadData(uint32_t address)
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
	uint8_t ret = QUADSPI->DR;
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
	return ret;
}


uint32_t ExtFlash::FastRead(uint32_t address)
{
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
	uint32_t ret = QUADSPI->DR;
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
	return ret;
}
