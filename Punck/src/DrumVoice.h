#pragma once

#include "initialisation.h"

// Abstract Class to define interface for drum voices

class DrumVoice {
public:
	float outputLevel[2];
	virtual void Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, uint8_t velocity) = 0;
	virtual void Play(uint8_t voice, uint32_t index) = 0;
	virtual void CalcOutput() = 0;
};

