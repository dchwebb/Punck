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
	void UpdateFilter() override;
	uint32_t SerialiseConfig(uint8_t* buff);
	void ReadConfig(uint8_t* buff, uint32_t len);

	NoteMapper* noteMapper;

private:
	Filter filter{4, BandPass, &(ADC_array[ADC_Filter_Pot])};

	float position;
	float velocityScale;
	float currentLevel;

	float carrierLevel;
	float carrierFreq = 2490.0f;
	float carrierPos;

	bool modulatorHigh;
	float modulatorFreq = 1047.0f;
	float modulatorDuty = 0.75f;
	float modulatorPos;
	float modulatorHighMult = 1.2f;
	float modulatorLowMult = 0.9f;
};
