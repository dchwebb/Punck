#pragma once
#include "initialisation.h"
#include "Filter.h"
#include "DrumVoice.h"

class NoteMapper;

class Kick : public DrumVoice {
public:
	void Play(const uint8_t voice, const uint32_t noteOffset, const uint32_t noteRange, const float velocity);
	void Play(const uint8_t voice, const uint32_t index);
	void CalcOutput();
	void UpdateFilter();
	uint32_t SerialiseConfig(uint8_t** buff, const uint8_t voiceIndex);
	void StoreConfig(uint8_t* buff, const uint32_t len);
	uint32_t ConfigSize();

	NoteMapper* noteMapper;

private:
	enum class Phase {Off, Ramp1, Ramp2, Ramp3, FastSine, SlowSine} phase;
	Filter<2> filter{filterPass::LowPass, &(ADC_array[ADC_KickAttack])};

	float position;
	float currentLevel;
	float velocityScale;

	float slowSinInc;
	float slowSinLevel;
	float fastSinInc = FreqToInc(config.fastSinFreq);

	struct Config {
		float ramp1Inc = 0.22f;					// Initial steep ramp
		float ramp2Inc = 0.015f;				// Second slower ramp
		float ramp3Inc = 0.3f;					// Steep Discontinuity leading to fast sine
		float fastSinFreq = 130.0f;				// Higher frequency of fast sine section
		float initSlowSinFreq = 60.0f;			// Initial frequency of gradually decreasing slow sine wave
		float sineSlowDownRate = 0.999985f;		// Rate at which slow sine wave frequency decreases
	} config;

};

