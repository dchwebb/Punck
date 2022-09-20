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
	enum class Phase {Off, Ramp, Ramp2, Ramp3, FastSine, SlowSine} phase;
	//Filter filter{2, LowPass, &(ADC_array[ADC_TomsFilter])};

	float position;
	float currentLevel;
	float velocityScale;

	float slowSinInc;
	float slowSinLevel;
	float pitchScale;

	struct Config {
		float ramp1Inc = 0.22f;					// Initial steep ramp
		float fastSinInc = 0.1f;				// Higher frequency of fast sine section
		float initSlowSinInc = 0.04f;			// Initial frequency of gradually decreasing slow sine wave
		float sineSlowDownRate = 0.9995f;		// Rate at which slow sine wave frequency decreases
	} config;

};

