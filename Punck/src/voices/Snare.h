#pragma once
#include "initialisation.h"
#include "DrumVoice.h"
#include "Filter.h"

class NoteMapper;



class Snare : public DrumVoice {
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
	static constexpr uint8_t partialCount = 3;
	float partialLevel[partialCount];
	float partialpos[partialCount];
	float partialInc[partialCount];

	float noiseLevel;
	float velocityScale;
	float sustainRate;

	struct Config {
		float noiseInitLevel = 1.0f;
		float noiseDecay = 0.9985f;				// Decay rate of noise (increased by decay amount in MIDI mode)

		float baseFreq = 150.0f;				// Lowest frequency which partials will be non-integer multiples of
		float basePos = 0.5f;					// Starting position of initial partial in radians

		float partialDecay = 0.9985f;			// Decay rate of partials (increased by decay amount in MIDI mode)
		float partialInitLevel[partialCount] = {0.7f, 0.5f, -0.4};
		float partialFreqOffset[partialCount] = {1.0f, 1.588f, 1.833f};
	} config;


	Filter<2> filter{filterPass::LowPass, &(ADC_array[ADC_SnareFilter])};		// Filters combined partial and noise elements of sound
};

