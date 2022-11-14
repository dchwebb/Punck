#pragma once

#include "initialisation.h"
#include "USB.h"


static uint32_t* const addrFlashBank2Sector7 = reinterpret_cast<uint32_t*>(0x081E0000);			 // Base address of Bank 2 Sector 7, 128 Kbytes
#define FLASH_ALL_ERRORS (FLASH_SR_WRPERR | FLASH_SR_PGSERR | FLASH_SR_STRBERR | FLASH_SR_INCERR | FLASH_SR_OPERR | FLASH_SR_RDPERR | FLASH_SR_RDSERR | FLASH_SR_SNECCERR | FLASH_SR_DBECCERR | FLASH_SR_CRCRDERR)

extern USB usb;


// Class used to store calibration settings - note this uses the Standard Peripheral Driver code
class Config {
public:
	static constexpr uint32_t configVersion = 1;

	bool scheduleSave = false;
	uint32_t saveBooked;
	uint8_t configBuffer[16384];		// Need a large buffer as drum sequence data is ~6k

	void Calibrate();
	void AutoZeroOffset();				// Automatically adjusts ADC zero offset by averaging low level signals
	void ScheduleSave();				// called whenever a config setting is changed to schedule a save after waiting to see if any more changes are being made
	bool SaveConfig();
	uint32_t SetConfig();				// Serialise configuration data into buffer
	void RestoreConfig();				// gets config from Flash, checks and updates settings accordingly

	void FlashUnlock(uint8_t bank);
	void FlashLock(uint8_t bank);
	void FlashEraseSector(uint8_t Sector, uint32_t bank);
	bool FlashWaitForLastOperation(uint32_t bank);
	bool FlashProgram(uint32_t* dest_addr, uint32_t* src_addr, size_t size);
private:


};

extern Config config;;
