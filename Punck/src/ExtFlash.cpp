#include "ExtFlash.h"
#include "ff.h"
#include "sdram_diskio.h"

// FIXME - canching currently disabled for testing; in memory mapped mode will want caching enabled and disabled for writes/erases

ExtFlash extFlash;

FATFS RAMDISKFatFs;					// File system object for RAM disk logical drive
FIL MyFile;							// File object
char RAMDISKPath[4];				// RAM disk logical drive path
FRESULT res;                                          // FatFs function common result code
uint32_t byteswritten, bytesread;                     // File write/read counts
uint8_t wtext[] = "This is STM32 working with FatFs"; // File write buffer
uint8_t rtext[100];                                   // File read buffer

void InitFatFS()
{
	// Link the RAM disk I/O driver
	if (FATFS_LinkDriver(&ExtFlashDriver, RAMDISKPath) == 0) {

		// Register the file system object to the FatFs module
		res = f_mount(&RAMDISKFatFs, (char const*)RAMDISKPath, 1);
		if (res == FR_NO_FILESYSTEM) {
			// Mount FAT file system on External Flash
			//f_mkfs((TCHAR const*)RAMDISKPath, (FM_ANY | FM_SFD), 0, fsWork, sizeof(fsWork));
		}
/*
		// Create a FAT file system (format) on the logical drive (Use SFD to optimise space - otherwise partition seems to start at sector 63)
		f_mkfs((TCHAR const*)RAMDISKPath, (FM_ANY | FM_SFD), 0, fsWork, sizeof(fsWork));

			// Create and Open a new text file object with write access
			if (f_open(&MyFile, "STM32.TXT", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK) {

				// Write data to the text file
				res = f_write(&MyFile, wtext, sizeof(wtext), (unsigned int*)&byteswritten);

				if ((byteswritten != 0) && (res == FR_OK)) {
					// Close the open text file
					f_close(&MyFile);

					// Open the text file object with read access
					if (f_open(&MyFile, "STM32.TXT", FA_READ) == FR_OK) {

						// Read data from the text file
						res = f_read(&MyFile, rtext, sizeof(rtext), (unsigned int *)&bytesread);

						if ((bytesread > 0) && (res == FR_OK)) {

							//Close the open text file
							f_close(&MyFile);
						}
					}
				}
			}
*/


//		DIR dp;						// Pointer to the directory object structure
//		FILINFO fno;				// File information structure
//		res = f_opendir(&dp, "");	// second parm is directory name (root)
//		res = f_readdir(&dp, &fno);
//		uint8_t dummy = 1;
	}
}

void ExtFlash::Init()
{
	InitQSPI();												// Initialise hardware

	// Quad SPI mode enable
	if ((ReadStatus(readStatusReg2) & 2) == 0) {			// QuadSPI not enabled
		WriteEnable();

		QUADSPI->CR |= QUADSPI_CR_EN;						// Enable QSPI
		QUADSPI->CCR = (QUADSPI_CCR_DMODE_0 |				// 00: No data; 01: Data on a single line; 10: Data on two lines; 11: Data on four lines
						QUADSPI_CCR_IMODE_0 |				// 00: No instruction; 01: Instruction on single line; 10: Two lines; 11: Four lines
						(writeStatusReg2 << QUADSPI_CCR_INSTRUCTION_Pos));
		QUADSPI->DR = 2;									// Set Quad Enable bit
		while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
		QUADSPI->CR &= ~QUADSPI_CR_EN;						// Disable QSPI
	}

	MemoryMapped();											// Switch to memory mapped mode
	InitFatFS();											// Initialise FatFS
}


void ExtFlash::MemoryMapped()
{
	// Activate memory mapped mode
	//SCB_InvalidateDCache_by_Addr(flashAddress, 10000);		// Ensure cache is refreshed after write or erase
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
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

	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	memMapMode = true;
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


void ExtFlash::WriteEnable()
{
	if (memMapMode) {
		QUADSPI->CR &= ~QUADSPI_CR_EN;						// Disable QSPI
		QUADSPI->CCR = 0;
		while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
		memMapMode = false;
	}
	CheckBusy();
	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI
	QUADSPI->CCR = (QUADSPI_CCR_IMODE_0 |					// 00: No instruction; 01: Instruction on single line; 10: Two lines; 11: Four lines
					(writeEnable << QUADSPI_CCR_INSTRUCTION_Pos));
	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
}


void ExtFlash::WriteData(uint32_t address, uint32_t* data, uint32_t words)
{
	do {
		WriteEnable();

		// Can write 256 bytes (64 words) at a time, and must be aligned to page boundaries (256 bytes)
		uint32_t startPage = (address >> 8);
		uint32_t endPage = (((address + (words * 4)) - 1) >> 8);

		uint32_t writeSize = words * 4;							// Size of current write in bytes
		if (endPage != startPage) {
			writeSize = ((startPage + 1) << 8) - address;		// When crossing pages only write up to the 256 byte boundary
		}

		QUADSPI->DLR = writeSize - 1;							// number of bytes - 1 to transmit
		QUADSPI->CR |= (3 << QUADSPI_CR_FTHRES_Pos);			// Set the threshold to 4 bytes
		QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI

		QUADSPI->CCR = (QUADSPI_CCR_ADSIZE_1 |					// Address: 00: 8-bit ; 01: 16-bit; *10: 24-bit; 11: 32-bit
						QUADSPI_CCR_ADMODE_0 |					// Address: 00: None; *01: One line; 10: Two lines; 11: Four lines
						QUADSPI_CCR_DMODE |						// Data: 00: None; 01: One line; 10: Two lines; *11: Four lines
						QUADSPI_CCR_IMODE_0 |					// Instruction: 00: None; *01: One line; 10: Two lines; 11: Four lines
						(quadPageProgram << QUADSPI_CCR_INSTRUCTION_Pos));
		QUADSPI->AR = address;
		for (uint8_t i = 0; i < (writeSize / 4); ++i) {
			QUADSPI->DR = *data++;
			while ((QUADSPI->SR & QUADSPI_SR_FTF) == 0) {};
		}

		words -= (writeSize / 4);
		address += writeSize;

		while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
		QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
	} while (words > 0);
}


void ExtFlash::SectorErase(uint32_t address)
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


void ExtFlash::CheckBusy()
{
	// Use Automatic Polling mode to wait until Flash Busy flag is cleared
	QUADSPI->DLR = 0;										// Return 1 byte
	QUADSPI->PSMKR = 0b00000001;							// Mask on bit 1 (Busy)
	QUADSPI->PSMAR = 0b00000000;							// Match Busy = 0
	QUADSPI->PIR = 0x10;									// Polling interval in clock cycles
	QUADSPI->CR |= QUADSPI_CR_APMS;							// Set the 'auto-stop' bit to end the transaction after a match.

	QUADSPI->CR |= QUADSPI_CR_EN;							// Enable QSPI

	QUADSPI->CCR = (QUADSPI_CCR_FMODE_1 |					// 00: Indirect write; 01: Indirect read; *10: Automatic polling; 11: Memory-mapped
					QUADSPI_CCR_DMODE_0 |					// Data: 00: None; *01: One line; 10: Two lines; 11: Four lines
					QUADSPI_CCR_IMODE_0 |					// Instruction: 00: None; *01: One line; 10: Two lines; 11: Four lines
					(readStatusReg1 << QUADSPI_CCR_INSTRUCTION_Pos));

	while (QUADSPI->SR & QUADSPI_SR_BUSY) {};
	QUADSPI->FCR |= QUADSPI_FCR_CSMF;						// Acknowledge status match flag
	QUADSPI->CR &= ~QUADSPI_CR_EN;							// Disable QSPI
}
