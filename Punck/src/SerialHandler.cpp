#include "SerialHandler.h"
#include "ExtFlash.h"

#include <stdio.h>

//extern Config config;

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

	} else if (ComCmd.compare("readreg\n") == 0) {				// Read QSPI register

		usb->SendString("Status register 1: " + std::to_string(extFlash.ReadStatus(ExtFlash::readStatusReg1)) +
				"\r\nStatus register 2: " + std::to_string(extFlash.ReadStatus(ExtFlash::readStatusReg2)) +
				"\r\nStatus register 3: " + std::to_string(extFlash.ReadStatus(ExtFlash::readStatusReg3)) + "\r\n");

	} else if (ComCmd.compare(0, 5, "write") == 0) {			// Write QSPI register (format writeA:D where A is address and D is data)

		int address = ParseInt(ComCmd, 'e', 0, 0xFFFFFF);
		if (address >= 0) {
			int data = ParseInt(ComCmd, ':', 0, 255);
			if (data >= 0) {
				extFlash.WriteData(address, data);
			}
		}

	} else if (ComCmd.compare(0, 5, "read:") == 0) {			// Read QSPI register (format read:A where A is address)

		int address = ParseInt(ComCmd, ':', 0, 0xFFFFFF);
		if (address >= 0) {
			usb->SendString("Data Read: " + std::to_string(extFlash.FastRead(address)) + "\r\n");
		}

	} else if (ComCmd.compare(0, 9, "mdlength:") == 0) {		// Modulated Delay length


//	} else if (ComCmd.compare("fdl\n") == 0) {					// Dump left filter buffer
//		suspendI2S();
//		uint16_t pos = filter.filterBuffPos[0];
//		for (int f = 0; f < filter.firTaps; ++f) {
//			usb->SendString(std::to_string(filter.filterBuffer[0][pos]) + "\r\n");
//			if (++pos == filter.firTaps)
//				pos = 0;
//		}
//		resumeI2S();

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

