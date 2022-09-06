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
	struct Config {
		float attackInc = 0.005f;
		float decay = 0.9991f;

		float hpInitCutoff = 0.07f;
		float hpFinalCutoff = 0.27f;

		float noiseInitLevel = 0.99f;
		float noiseDecay = 0.9999f;

		float partialScale[6] = {0.8f, 0.5f, 0.4f, 0.4f, 0.5f, 0.4f};
		float partialFreq[6] = {-569.0f, 621.0f, -1559.0f, 2056.0f, -3300.0f, 5515.0f};
	} config;


	Filter hpFilter{2, HighPass, &(ADC_array[ADC_Filter_Pot])};
	Filter lpFilter{1, LowPass, &(ADC_array[ADC_Filter_Pot])};
	//BPFilter bpFilter;

	float velocityScale;
	float decayScale;
	float currentLevel;
	bool attack;
	float attackLevel;
	float noiseScale;

	float hpFilterCutoff;

	// multiplier to convert frequency to half a period of a triangle wave
	static constexpr float freqToTriPeriod = 1 / ((float)systemSampleRate / 4.0f);
	float partialInc[6] = {
			  -569.0f * freqToTriPeriod,
			   621.0f * freqToTriPeriod,
			 -1559.0f * freqToTriPeriod,
			  2056.0f * freqToTriPeriod,
			 -3300.0f * freqToTriPeriod,
			  5515.0f * freqToTriPeriod,
			};
	float partialLevel[6] = {};


};
