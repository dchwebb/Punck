#pragma once
#include "initialisation.h"
#include "DrumVoice.h"
#include "Filter.h"

class NoteMapper;

constexpr float FreqToInc(float frequency)
{
	return frequency * (2 * pi) / systemSampleRate;
}

class Snare : public DrumVoice {
public:
	void Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, float velocity);
	void Play(uint8_t voice, uint32_t index);
	void CalcOutput();
	void UpdateFilter();

	NoteMapper* noteMapper;
private:
	static constexpr uint8_t partialCount = 3;
	float partialLevel[partialCount];
	float partialInitLevel[partialCount] = {0.7f, -0.4f, -0.5};
	float partialFreqOffset[partialCount] = {1.0f, 1.588f, 1.833f};
	float partialpos[partialCount];
	float partialPitchDrop = 1.0f;			// FIXME - 1 = disabled not sure this really adds anything
	float partialDecay = 0.9991f;			// Decay rate of partials (increased by decay adc amount)

	float baseFreq = 150.0f;				// Lowest frequency which partials will be non-integer multiples of
	float partialInc[partialCount] = {
			FreqToInc(baseFreq * partialFreqOffset[0]),
			FreqToInc(baseFreq * partialFreqOffset[1]),
			FreqToInc(baseFreq * partialFreqOffset[2])};

	float noiseInitLevel = 1.0f;
	float noiseLevel;
	float noiseDecay = 0.999f;				// Decay rate of noise (increased by decay adc amount)

	float velocityScale;

	bool playing = false;
	Filter filter{2, LowPass, &(ADC_array[ADC_Filter_Pot])};		// Filters combined partial and noise elements of sound
};

