#pragma once
#include "initialisation.h"
#include "Filter.h"
#include "DrumVoice.h"

class NoteMapper;
class HiHat  : public DrumVoice {
public:
	void Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, float velocity);
	void Play(uint8_t voice, uint32_t index);
	void CalcOutput();
	void UpdateFilter();
	uint32_t SerialiseConfig(uint8_t* buff);
	void ReadConfig(uint8_t* buff, uint32_t len);

	NoteMapper* noteMapper;

private:
	Filter filter{2, LowPass, &(ADC_array[ADC_Filter_Pot])};

	float position;
	float velocityScale;
	float currentLevel;
};
