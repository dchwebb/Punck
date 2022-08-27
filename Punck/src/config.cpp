#include <config.h>
#include <cmath>

Config config;

// called whenever a config setting is changed to schedule a save after waiting to see if any more changes are being made
void Config::ScheduleSave()
{
	scheduleSave = true;
	saveBooked = SysTickVal;
}


// Write calibration settings to Flash memory (H743 see programming manual p152 for sector layout)
bool Config::SaveConfig()
{
	scheduleSave = false;
	suspendI2S();

	configValues cv;
	SetConfig(cv);

	__disable_irq();					// Disable Interrupts
	FlashUnlock(2);						// Unlock Flash memory for writing
	FLASH->SR2 = FLASH_ALL_ERRORS;		// Clear error flags in Status Register
	FlashEraseSector(7, 2);				// Erase sector 7, Bank 2 (See p152 of manual)
	bool result = FlashProgram(ADDR_FLASH_SECTOR_7, reinterpret_cast<uint32_t*>(&cv), sizeof(cv));
	FlashLock(2);						// Lock Bank 2 Flash
	__enable_irq(); 					// Enable Interrupts

	resumeI2S();
	return result;
}


void Config::SetConfig(configValues &cv)
{
//	cv.audio_offset_left = adcZeroOffset[left];
//	cv.audio_offset_right = adcZeroOffset[right];
}


// Restore configuration settings from flash memory
void Config::RestoreConfig()
{
	// create temporary copy of settings from memory to check if they are valid
	configValues cv;
	memcpy(reinterpret_cast<uint32_t*>(&cv), ADDR_FLASH_SECTOR_7, sizeof(cv));

	if (strcmp(cv.StartMarker, "CFG") == 0 && strcmp(cv.EndMarker, "END") == 0 && cv.Version == CONFIG_VERSION) {
//		adcZeroOffset[left]  = cv.audio_offset_left;
//		adcZeroOffset[right] = cv.audio_offset_right;
	}

	// Set up averaging values for ongoing ADC offset calibration
//	newOffset[0] = adcZeroOffset[0];
//	newOffset[1] = adcZeroOffset[1];
}


// Unlock the FLASH control register access
void __attribute__((section(".itcm_text"))) Config::FlashUnlock(uint8_t bank)
{
	volatile uint32_t* bankCR  = &(bank == 1 ? FLASH->CR1 : FLASH->CR2);
	volatile uint32_t* bankKEY = &(bank == 1 ? FLASH->KEYR1 : FLASH->KEYR2);

	if ((*bankCR & FLASH_CR_LOCK) != 0)  {
		*bankKEY = 0x45670123U;					// These magic numbers unlock the bank for programming
		*bankKEY = 0xCDEF89ABU;
	}
}


// Lock the FLASH Bank Registers access
void __attribute__((section(".itcm_text"))) Config::FlashLock(uint8_t bank)
{
	if (bank == 1) {
		FLASH->CR1 |= FLASH_CR_LOCK;
	} else {
		FLASH->CR2 |= FLASH_CR_LOCK;
	}
}


// Erase sector - FLASH_CR_PSIZE_1 means a write size of 32bits (see p163 of manual)
void __attribute__((section(".itcm_text"))) Config::FlashEraseSector(uint8_t Sector, uint32_t bank)
{
	volatile uint32_t* bankCR = &(bank == 1 ? FLASH->CR1 : FLASH->CR2);

	*bankCR &= ~(FLASH_CR_PSIZE | FLASH_CR_SNB);
	*bankCR |= (FLASH_CR_SER |					// Sector erase request
			FLASH_CR_PSIZE_1 |					// Write 32 bits at a time
			(Sector << FLASH_CR_SNB_Pos) |
			FLASH_CR_START);
}


bool __attribute__((section(".itcm_text"))) Config::FlashWaitForLastOperation(uint32_t bank)
{
    // Even if FLASH operation fails, the QW flag will be reset and an error flag will be set
	volatile uint32_t* bankSR  = &(bank == 1 ? FLASH->SR1 : FLASH->SR2);
	volatile uint32_t* bankCCR = &(bank == 1 ? FLASH->CCR1 : FLASH->CCR2);

	if (*bankSR & FLASH_ALL_ERRORS) {					// If any error occurred abort
		*bankSR = FLASH_ALL_ERRORS;						// Clear all errors
		return false;
	}

	while ((*bankSR & FLASH_SR_QW) == FLASH_SR_QW) {	// QW flag set when write or erase operation is pending in the command queue buffer

	}

	if ((*bankSR & FLASH_SR_EOP) == FLASH_SR_EOP) {		// Check End of Operation flag
		*bankCCR = FLASH_CCR_CLR_EOP;
	}

	return true;
}


bool __attribute__((section(".itcm_text"))) Config::FlashProgram(uint32_t* dest_addr, uint32_t* src_addr, size_t size)
{
	uint8_t bank = (reinterpret_cast<uintptr_t>(dest_addr) < FLASH_BANK2_BASE) ? 1 : 2;		// Get which bank we are programming from destination address
	volatile uint32_t* bankCR = &(bank == 1 ? FLASH->CR1 : FLASH->CR2);

	if (Config::FlashWaitForLastOperation(bank)) {
		*bankCR |= FLASH_CR_PG;

		__ISB();
		__DSB();

		// Each write block is 32 bytes
		for (uint16_t b = 0; b < std::ceil(static_cast<float>(size) / 32); ++b) {
			for (uint8_t i = 0; i < FLASH_NB_32BITWORD_IN_FLASHWORD; ++i) {
				*dest_addr = *src_addr;
				dest_addr++;
				src_addr++;
			}

			if (!Config::FlashWaitForLastOperation(bank)) {
				*bankCR &= ~FLASH_CR_PG;				// Clear programming flag
				return false;
			}
		}

		__ISB();
		__DSB();

		*bankCR &= ~FLASH_CR_PG;						// Clear programming flag
	}
	return true;
}


