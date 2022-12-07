#include <configManager.h>
#include <cmath>
#include "VoiceManager.h"
#include "Sequencer.h"
#include "Reverb.h"

Config configManager;

// called whenever a config setting is changed to schedule a save after waiting to see if any more changes are being made
void Config::ScheduleSave()
{
	scheduleSave = true;
	saveBooked = SysTickVal;
}


// Write calibration settings to Flash memory (H743 see programming manual p152 for sector layout)
bool Config::SaveConfig(bool eraseOnly)
{
	// Set all LEDs to full
	voiceManager.SetAllLeds(1.0f);

	scheduleSave = false;
	bool result = true;

	suspendI2S();

	uint32_t cfgSize = SetConfig();

	__disable_irq();					// Disable Interrupts
	FlashUnlock(2);						// Unlock Flash memory for writing
	FLASH->SR2 = FLASH_ALL_ERRORS;		// Clear error flags in Status Register

	// Check if flash needs erasing
	for (uint32_t i = 0; i < BufferSize / 4; ++i) {
		if (addrFlashBank2Sector7[i] != 0xFFFFFFFF) {
			FlashEraseSector(7, 2);		// Erase sector 7, Bank 2 (See p152 of manual)
			break;
		}
	}

	if (!eraseOnly) {
		result = FlashProgram(addrFlashBank2Sector7, reinterpret_cast<uint32_t*>(&configBuffer), cfgSize);
	}

	FlashLock(2);						// Lock Bank 2 Flash
	voiceManager.RestoreAllLeds();
	__enable_irq(); 					// Enable Interrupts
	resumeI2S();

	return result;
}


uint32_t Config::SetConfig()
{
	// Serialise config values into buffer
	memset(configBuffer, 0, sizeof(configBuffer));					// Clear buffer
	strncpy(reinterpret_cast<char*>(configBuffer), "CFG", 4);		// Header
	configBuffer[4] = configVersion;
	uint32_t configPos = 8;											// Position in buffer to store data
	uint32_t configSize = 0;										// Holds the size of each config buffer

	uint8_t* cfgBuffer = nullptr;

	// Voice configurations - store each one into config buffer
	for (auto& nm : voiceManager.noteMapper) {
		if (nm.drumVoice != nullptr && nm.voiceIndex == 0) {		// If voiceIndex is > 0 drum voice has multiple channels (eg sampler)

			configSize = nm.drumVoice->SerialiseConfig(&cfgBuffer, nm.voiceIndex);
			memcpy(&configBuffer[configPos], cfgBuffer, configSize);
			configPos += configSize;
		}
	}

	// Reverb settings
	configSize = reverb.SerialiseConfig(&cfgBuffer);
	memcpy(&configBuffer[configPos], cfgBuffer, configSize);
	configPos += configSize;

	// MIDI note map
	configSize = voiceManager.GetConfig(&cfgBuffer);
	memcpy(&configBuffer[configPos], cfgBuffer, configSize);
	configPos += configSize;

	// Drum sequences
	configSize = sequencer.GetSequences(&cfgBuffer);
	memcpy(&configBuffer[configPos], cfgBuffer, configSize);
	configPos += configSize;

	// Footer
	strncpy(reinterpret_cast<char*>(&configBuffer[configPos]), "END", 4);
	configPos += 4;
	return configPos;
}


// Restore configuration settings from flash memory
void Config::RestoreConfig()
{
	uint8_t* flashConfig = reinterpret_cast<uint8_t*>(addrFlashBank2Sector7);

	// Check for config start and version number
	if (strcmp((char*)flashConfig, "CFG") == 0 && *reinterpret_cast<uint32_t*>(&flashConfig[4]) == configVersion) {
		uint32_t configPos = 8;											// Position in buffer to store data

		// Voice configurations - store each one into config buffer
		for (auto& nm : voiceManager.noteMapper) {
			if (nm.voiceIndex == 0) {		// If voiceIndex is > 0 drum voice has multiple channels (eg sampler)
				const uint32_t configSize = nm.drumVoice->ConfigSize();
				nm.drumVoice->StoreConfig(&flashConfig[configPos], configSize);
				configPos += configSize;
			}
		}

		// Reverb Settings
		configPos += reverb.StoreConfig(&flashConfig[configPos]);

		// MIDI note map
		configPos += voiceManager.StoreConfig(&flashConfig[configPos]);

		// Drum sequences
		sequencer.StoreSequences(&flashConfig[configPos]);
		configPos += sequencer.SequencesSize();
	} else {
		// Call Store Config to initialise values as required
		for (auto& nm : voiceManager.noteMapper) {
			if (nm.voiceIndex == 0) {		// If voiceIndex is > 0 drum voice has multiple channels (eg sampler)
				nm.drumVoice->StoreConfig(nullptr, 0);
			}
		}

		// Reverb Settings
		reverb.StoreConfig(nullptr);

	}
}



// Unlock the FLASH control register access
void Config::FlashUnlock(uint8_t bank)
{
	volatile uint32_t* bankCR  = &(bank == 1 ? FLASH->CR1 : FLASH->CR2);
	volatile uint32_t* bankKEY = &(bank == 1 ? FLASH->KEYR1 : FLASH->KEYR2);

	if ((*bankCR & FLASH_CR_LOCK) != 0)  {
		*bankKEY = 0x45670123U;					// These magic numbers unlock the bank for programming
		*bankKEY = 0xCDEF89ABU;
	}
}


// Lock the FLASH Bank Registers access
void Config::FlashLock(uint8_t bank)
{
	if (bank == 1) {
		FLASH->CR1 |= FLASH_CR_LOCK;
	} else {
		FLASH->CR2 |= FLASH_CR_LOCK;
	}
}


// Erase sector - FLASH_CR_PSIZE_1 means a write size of 32bits (see p163 of manual)
void Config::FlashEraseSector(uint8_t Sector, uint32_t bank)
{
	volatile uint32_t* bankCR = &(bank == 1 ? FLASH->CR1 : FLASH->CR2);

	*bankCR &= ~(FLASH_CR_PSIZE | FLASH_CR_SNB);
	*bankCR |= (FLASH_CR_SER |							// Sector erase request
			FLASH_CR_PSIZE_1 |							// Write 32 bits at a time
			(Sector << FLASH_CR_SNB_Pos) |
			FLASH_CR_START);
}


bool Config::FlashWaitForLastOperation(uint32_t bank)
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


bool Config::FlashProgram(uint32_t* dest_addr, uint32_t* src_addr, size_t size)
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


