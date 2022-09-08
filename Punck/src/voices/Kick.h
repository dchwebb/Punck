#pragma once
#include "initialisation.h"
#include "Filter.h"
#include "DrumVoice.h"

class NoteMapper;

class Kick : public DrumVoice {
public:
	void Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, float velocity);
	void Play(uint8_t voice, uint32_t index);
	void CalcOutput();
	void UpdateFilter();
	uint32_t SerialiseConfig(uint8_t* buff);
	void ReadConfig(uint8_t* buff, uint32_t len);

	NoteMapper* noteMapper;

private:
	enum class Phase {Off, Ramp1, Ramp2, Ramp3, FastSine, SlowSine} phase;
	Filter filter{2, LowPass, &(ADC_array[ADC_Filter_Pot])};

	float position;
	float currentLevel;
	float velocityScale;

	float slowSinInc;
	float slowSinLevel;

	struct Config {
		float ramp1Inc = 0.22f;					// Initial steep ramp
		float ramp2Inc = 0.015f;				// Second slower ramp
		float ramp3Inc = 0.3f;					// Steep Discontinuity leading to fast sine
		float fastSinInc = 0.017f;				// Higher frequency of fast sine section
		float initSlowSinInc = 0.00785f;		// Initial frequency of gradually decreasing slow sine wave
		float sineSlowDownRate = 0.999985f;		// Rate at which slow sine wave frequency decreases
	} config;

};

