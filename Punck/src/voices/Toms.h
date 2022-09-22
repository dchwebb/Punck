#pragma once
#include "initialisation.h"
#include "Filter.h"
#include "DrumVoice.h"

class NoteMapper;

class Toms : public DrumVoice {
public:
	void Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, float velocity);
	void Play(uint8_t voice, uint32_t index);
	void CalcOutput();
	void UpdateFilter();
	uint32_t SerialiseConfig(uint8_t** buff, uint8_t voiceIndex);
	void StoreConfig(uint8_t* buff, uint32_t len);

	NoteMapper* noteMapper;

private:
	enum class Phase {Off, Ramp, Sine} phase;
	//Filter filter{2, LowPass, &(ADC_array[ADC_TomsFilter])};

	float position;
	float currentLevel;
	float velocityScale;

	float sineInc;
	float sineLevel;
	float position2, sineInc2, sineLevel2;
	float pitchScale;

	struct Config {
		float decaySpeed = 0.9996f;
		float decaySpeed2 = 0.9998f;
		float rampInc = 0.22f;					// Initial steep ramp
		float initSineFreq = 145.0f;			// Initial frequency of gradually decreasing sine wave
		float sineFreq2scale = 1.588f;			// Initial frequency of gradually decreasing sine wave
		float sineLevel2 = 0.6f;
		float sineSlowDownRate = 0.9998f;		// Rate at which sine wave frequency decreases
	} config;

};

