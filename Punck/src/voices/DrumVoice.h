#pragma once

#include "initialisation.h"

// Abstract Class to define interface for drum voices

class DrumVoice {
public:
	float outputLevel[2];
	bool playing;

	virtual void Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, float velocity) = 0;
	virtual void Play(uint8_t voice, uint32_t index) = 0;
	virtual void CalcOutput() = 0;
	virtual uint32_t SerialiseConfig(uint8_t** buff) = 0;		// Fills a buffer with config data for transmission over SysEx
	virtual void ReadConfig(uint8_t* buff, uint32_t len) = 0;				// Reads config data back into member values
	virtual void UpdateFilter() {};
};

