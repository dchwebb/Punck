#pragma once
#include "initialisation.h"
#include "Filter.h"
#include "DrumVoice.h"

class NoteMapper;

class Kick : public DrumVoice {
public:
	void Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange);
	void Play(uint8_t voice, uint32_t index);
	void CalcOutput();
	void UpdateFilter();

	NoteMapper* noteMapper;

private:
	enum class Phase {Off, Ramp1, Ramp2, Ramp3, FastSine, SlowSine} phase;
	float position;

	float ramp1Inc = 0.22f;					// Initial steep ramp
	float ramp2Inc = 0.015f;				// Second slower ramp
	float ramp3Inc = 0.3f;					// Steep Discontinuity leading to fast sine
	float fastSinInc = 0.017f;				// Higher frequency of fast sine section
	float initSlowSinInc = 0.00785f;		// Initial frequency of gradually decreasing slow sine wave
	float sineSlowDownRate = 0.999985f;		// Rate at which slow sine wave frequency decreases

	float slowSinInc;
	float slowSinLevel;

	Filter filter{2, LowPass, &(ADC_array[ADC_Filter_Pot])};
	float currentLevel;
};

