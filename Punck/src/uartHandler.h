#pragma once

#include "initialisation.h"
#include <string>
#include <sstream>
#include <iomanip>


class UART {
public:
	bool commandReady = false;
	uint8_t cmdPos = 0;
	char command[255];

	void Init();
	void SendString(const std::string& s);
	void SendString(const char* s);
	void SendChar(char c);
	void ProcessCommand();

	std::string IntToString(const int32_t& v);
	std::string HexToString(const uint32_t& v, const bool& spaces);
	std::string HexByte(const uint16_t& v);
private:


};

extern UART uart;
