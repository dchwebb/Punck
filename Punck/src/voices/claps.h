#pragma once
#include "initialisation.h"
#include "DrumVoice.h"
#include "Filter.h"

class NoteMapper;



class Claps : public DrumVoice {
public:
	void Play(const uint8_t voice, const uint32_t noteOffset, uint32_t noteRange, const float velocity);
	void Play(const uint8_t voice, const uint32_t index);
	void CalcOutput();
	void UpdateFilter();
	uint32_t SerialiseConfig(uint8_t** buff, const uint8_t voiceIndex);
	void StoreConfig(uint8_t* buff, const uint32_t len);
	uint32_t ConfigSize();

	NoteMapper* noteMapper;
private:

	float level;
	float velocityScale;

	struct Config {
		float initLevel = 1.0f;
		float decay = 0.9980f;				// Decay rate
	} config;


//	Filter filter{2, filterPass::LowPass, &(ADC_array[ADC_SnareFilter])};		// Filters combined partial and noise elements of sound
};

