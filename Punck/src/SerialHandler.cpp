#include "SerialHandler.h"
#include "ExtFlash.h"

#include <stdio.h>

//extern Config config;

uint32_t flashBuff[1024];

int32_t SerialHandler::ParseInt(const std::string cmd, const char precedingChar, int low = 0, int high = 0) {
	int32_t val = -1;
	int8_t pos = cmd.find(precedingChar);		// locate position of character preceding
	if (pos >= 0 && std::strspn(cmd.substr(pos + 1).c_str(), "0123456789-") > 0) {
		val = stoi(cmd.substr(pos + 1));
	}
	if (high > low && (val > high || val < low)) {
		usb->SendString("Must be a value between " + std::to_string(low) + " and " + std::to_string(high) + "\r\n");
		return low - 1;
	}
	return val;
}

float SerialHandler::ParseFloat(const std::string cmd, const char precedingChar, float low = 0.0, float high = 0.0) {
	float val = -1.0f;
	int8_t pos = cmd.find(precedingChar);		// locate position of character preceding
	if (pos >= 0 && std::strspn(cmd.substr(pos + 1).c_str(), "0123456789.") > 0) {
		val = stof(cmd.substr(pos + 1));
	}
	if (high > low && (val > high || val < low)) {
		usb->SendString("Must be a value between " + std::to_string(low) + " and " + std::to_string(high) + "\r\n");
		return low - 1.0f;
	}
	return val;
}

SerialHandler::SerialHandler(USB& usbObj)
{
	usb = &usbObj;

	// bind the usb's CDC caller to the CDC handler in this class
	usb->cdcDataHandler = std::bind(&SerialHandler::Handler, this, std::placeholders::_1, std::placeholders::_2);
}



// Check if a command has been received from USB, parse and action as required
bool SerialHandler::Command()
{
	//char buf[50];

	if (!CmdPending) {
		return false;
	}

	if (ComCmd.compare("info\n") == 0) {		// Print diagnostic information

		usb->SendString("Mountjoy Punck v1.0 - Current Settings:\r\n\r\n");

	} else if (ComCmd.compare("help\n") == 0) {

		usb->SendString("Mountjoy Punck\r\n"
				"\r\nSupported commands:\r\n"
				"info        -  Show diagnostic information\r\n"
				"resume      -  Resume I2S after debugging\r\n"
				"memmap      -  QSPI flash to memory mapped mode\r\n"
				"readreg     -  Print QSPI flash status registers\r\n"
				"writeA:N    -  Write byte to flash (A = address, N = No of words in decimal)\r\n"
				"read:A      -  Read word from flash (A = decimal address)\r\n"
				"printflash:A   Print 100 words of flash (A = decimal address)\r\n"
				"erasesect:A    Erase flash sector (A = decimal address)\r\n"
				"\r\n"
#if (USB_DEBUG)
				"usbdebug    -  Start USB debugging\r\n"
				"\r\n"
#endif
		);

#if (USB_DEBUG)
	} else if (ComCmd.compare("usbdebug\n") == 0) {				// Configure gate LED
		USBDebug = true;
		usb->SendString("Press link button to dump output\r\n");
#endif

	} else if (ComCmd.compare("memmap\n") == 0) {				// QSPI flash to memory mapped mode
		extFlash.MemoryMapped();
		usb->SendString("Changed to memory mapped mode\r\n");

	} else if (ComCmd.compare(0, 11, "printflash:") == 0) {		// QSPI flash: print memory mapped data
		int address = ParseInt(ComCmd, ':', 0, 0xFFFFFF);
		if (address >= 0) {
			uint32_t* p = (uint32_t*)(0x90000000 + address);

			for (uint8_t a = 0; a < 100; ++a) {
				printf("%d: %#010x\r\n", (a * 4) + address, *p++);
			}
		}
	} else if (ComCmd.compare( 0, 10, "erasesect:") == 0) {		// Erase sector of flash memory
		int address = ParseInt(ComCmd, ':', 0, 0xFFFFFF);
		if (address >= 0) {
			extFlash.SectorErase(address);
			usb->SendString("Sector erased\r\n");
		}
		extFlash.MemoryMapped();

	} else if (ComCmd.compare("readreg\n") == 0) {				// Read QSPI register

		usb->SendString("Status register 1: " + std::to_string(extFlash.ReadStatus(ExtFlash::readStatusReg1)) +
				"\r\nStatus register 2: " + std::to_string(extFlash.ReadStatus(ExtFlash::readStatusReg2)) +
				"\r\nStatus register 3: " + std::to_string(extFlash.ReadStatus(ExtFlash::readStatusReg3)) + "\r\n");

	} else if (ComCmd.compare(0, 5, "write") == 0) {			// Write QSPI format writeA:W [A = address; W = num words]

		int address = ParseInt(ComCmd, 'e', 0, 0xFFFFFF);
		if (address >= 0) {
			int words = ParseInt(ComCmd, ':');
			printf("Writing %d words to %d ...\r\n", words, address);

			for (int a = 0; a < words; ++a) {
				flashBuff[a] = a + 1;
			}
			extFlash.WriteData(address, flashBuff, words);

			extFlash.MemoryMapped();
			printf("Finished\r\n");
		}


	} else if (ComCmd.compare(0, 5, "read:") == 0) {			// Read QSPI data (format read:A where A is address)

		int address = ParseInt(ComCmd, ':', 0, 0xFFFFFF);
		if (address >= 0) {
			printf("Data Read: %#010x\r\n", (unsigned int)extFlash.FastRead(address));
		}


	} else {
		usb->SendString("Unrecognised command: " + ComCmd + "Type 'help' for supported commands\r\n");
	}

	CmdPending = false;
	return true;
}


void SerialHandler::Handler(uint8_t* data, uint32_t length)
{
	static bool newCmd = true;
	if (newCmd) {
		ComCmd = std::string(reinterpret_cast<char*>(data), length);
		newCmd = false;
	} else {
		ComCmd.append(reinterpret_cast<char*>(data), length);
	}
	if (*ComCmd.rbegin() == '\r')
		*ComCmd.rbegin() = '\n';

	if (*ComCmd.rbegin() == '\n') {
		CmdPending = true;
		newCmd = true;
	}

}

