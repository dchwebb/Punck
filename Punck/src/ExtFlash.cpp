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
	QUADSPI->CCR = (QUADSPI_CCR_FMODE |						// 00: Indirect write mode; 01: Indirect read mode; 10: Automatic polling mode; 11: Memory-mapped mode
					QUADSPI_CCR_ADSIZE_1 |					// Address: 00: 8-bit ; 01: 16-bit; *10: 24-bit; 11: 32-bit
					QUADSPI_CCR_ADMODE_0 |					// 00: No address; 01: Address on single line; 10: Address on two lines; 11: Address on four lines
					QUADSPI_CCR_DMODE |						// 00: No data; 01: Data on single line; 10: Data on two lines; 11: Data on four lines
					QUADSPI_CCR_IMODE_0|					// 00: No instruction; 01: Instruction on single line; 10: Two lines; 11: Four lines
					(8 << QUADSPI_CCR_DCYC_Pos));			// insert 8 dummy clock cycles

	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI

}

uint8_t ExtFlash::ReadStatus(qspiRegister r)
{
	QUADSPI->CCR = (QUADSPI_CCR_FMODE_0 |					// 00: Indirect write mode; 01: Indirect read mode; 10: Automatic polling mode; 11: Memory-mapped mode
					 QUADSPI_CCR_DMODE_0 |					// 00: No data; 01: Data on a single line; 10: Data on two lines; 11: Data on four lines
					QUADSPI_CCR_IMODE_0);					// 00: No instruction; 01: Instruction on single line; 10: Two lines; 11: Four lines
	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI
	QUADSPI->CCR |=  (r << QUADSPI_CCR_INSTRUCTION_Pos);	// Read status register 1
	while ((QUADSPI->SR & QUADSPI_SR_TCF) == 0) {};			// Wait until transfer complete
	uint8_t ret = QUADSPI->DR;
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
	return ret;
}


void ExtFlash::WriteData(uint32_t address, uint8_t data)
{
	QUADSPI->CCR = (QUADSPI_CCR_ADSIZE_1 |					// Address: 00: 8-bit ; 01: 16-bit; *10: 24-bit; 11: 32-bit
					QUADSPI_CCR_ADMODE_0 |					// 00: No address; 01: Address on single line; 10: Address on two lines; 11: Address on four lines
					QUADSPI_CCR_DMODE_0 |					// 00: No data; 01: Data on single line; 10: Data on two lines; 11: Data on four lines
					QUADSPI_CCR_IMODE_0);					// 00: No instruction; 01: Instruction on single line; 10: Two lines; 11: Four lines
	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI
	QUADSPI->CCR |= (pageProgram << QUADSPI_CCR_INSTRUCTION_Pos);
	QUADSPI->AR = address;
	QUADSPI->DR = data;
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
}


uint8_t ExtFlash::ReadData(uint32_t address)
{
	QUADSPI->CCR = (QUADSPI_CCR_FMODE_0 |					// 00: Indirect write mode; *01: Indirect read mode; 10: Automatic polling mode; 11: Memory-mapped mode
					QUADSPI_CCR_ADSIZE_1 |					// Address: 00: 8-bit ; 01: 16-bit; *10: 24-bit; 11: 32-bit
					QUADSPI_CCR_ADMODE_0 |					// 00: No address; 01: Address on single line; 10: Address on two lines; 11: Address on four lines
					QUADSPI_CCR_DMODE_0 |					// 00: No data; 01: Data on single line; 10: Data on two lines; 11: Data on four lines
					QUADSPI_CCR_IMODE_0);					// 00: No instruction; 01: Instruction on single line; 10: Two lines; 11: Four lines
	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI
	QUADSPI->CCR |= (readData << QUADSPI_CCR_INSTRUCTION_Pos);
	QUADSPI->AR = address;

	while ((QUADSPI->SR & QUADSPI_SR_TCF) == 0) {};			// Wait until transfer complete
	uint8_t ret = QUADSPI->DR;
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
	return ret;
}


uint32_t ExtFlash::FastRead(uint32_t address)
{
	QUADSPI->CCR = (QUADSPI_CCR_FMODE_0 |					// 00: Indirect write mode; *01: Indirect read mode; 10: Automatic polling mode; 11: Memory-mapped mode
					QUADSPI_CCR_ADSIZE_1 |					// Address: 00: 8-bit ; 01: 16-bit; *10: 24-bit; 11: 32-bit
					QUADSPI_CCR_ADMODE |					// 00: No address; 01: Address on single line; 10: Address on two lines; 11: Address on four lines
					QUADSPI_CCR_ABMODE |					// 00: No AB; 01: AB on single line; 10: AB on two lines; 11: AB on four lines
					QUADSPI_CCR_DMODE |						// 00: No data; 01: Data on single line; 10: Data on two lines; 11: Data on four lines
					QUADSPI_CCR_IMODE_0 |
					(4 << QUADSPI_CCR_DCYC_Pos));			// insert 8 dummy clock cycles
	QUADSPI->DLR = 0x3;										// Return 4 bytes
	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI
	QUADSPI->CCR |= (fastReadIO << QUADSPI_CCR_INSTRUCTION_Pos);
	QUADSPI->ABR = 0xFF;									// Use alternate bytes to pad the address with a dummy byte of 0xFF
	QUADSPI->AR = address;									// Address needs to be four bytes with a dummy byte of 0xFF

	while ((QUADSPI->SR & QUADSPI_SR_TCF) == 0) {};			// Wait until transfer complete
	uint32_t ret = QUADSPI->DR;
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
	return ret;
}
