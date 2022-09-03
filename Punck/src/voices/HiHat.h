#pragma once
#include "initialisation.h"
#include "Filter.h"
#include "BPFilter.h"
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
	Filter hpFilter{4, HighPass, &(ADC_array[ADC_Filter_Pot])};
	BPFilter bpFilter;

	float position;
	float velocityScale;
	float currentLevel;

	float carrierLevel;
	float carrierPos;

	bool modulatorHigh;
	float modulatorPos;

	struct Config {
		float carrierFreq = 2490.0f;
		float modulatorFreq = 1047.0f;
		float modulatorDuty = 0.75f;
		float modulatorHighMult = 3.2f;
		float modulatorLowMult = 0.7f;
		float decay = 1.0f;	//0.999f;
		float Q = 7.0f;
	} config;
};
