#pragma once

#include "initialisation.h"

// Abstract Class to define interface for drum voices

class DrumVoice {
public:
	float outputLevel[2];
	bool playing;

	virtual void Play(const uint8_t voice, const uint32_t noteOffset, const uint32_t noteRange, const float velocity) = 0;
	virtual void Play(const uint8_t voice, const uint32_t index) = 0;
	virtual void CalcOutput() = 0;
	virtual uint32_t SerialiseConfig(uint8_t** buff, const uint8_t voiceIndex) = 0;		// Fills a buffer with config data for transmission over SysEx
	virtual void StoreConfig(uint8_t* buff, const uint32_t len) = 0;					// Reads config data back into member values
	virtual void UpdateFilter() {};

	constexpr float FreqToInc(const float frequency)
	{
		return frequency * (2 * pi) / systemSampleRate;
	}
};

