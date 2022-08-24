#pragma once
#include "initialisation.h"
#include "DrumVoice.h"
#include "Filter.h"

class NoteMapper;

constexpr float FrequencyToIncrement(float frequency)
{
	return frequency * (2 * pi) / systemSampleRate;
}

class Snare : public DrumVoice {
public:
	void Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange);
	void Play(uint8_t voice, uint32_t index);
	void CalcOutput();
	void UpdateFilter();

	NoteMapper* noteMapper;
private:
	float partial1Freq = 150.0f;				// Relative frequency of partial 2 to partial 1

	float currentLevel;
	float partial1Level;
	float partial2Level;
	float randLevel;
	float partial1InitLevel = 0.7f;
	float partial2InitLevel = -0.5f;
	float randInitLevel = 1.0f;
	float randDecay = 0.999f;
	float partial1Inc = FrequencyToIncrement(partial1Freq);					// First Mode 0,1 frequency
	float partial2Inc = FrequencyToIncrement(partial1Freq * 1.833f);		// Second Mode 0,1 frequency
	float partial1pos;
	float partial2pos;

	bool playing = false;
	Filter filter{2, LowPass, &(ADC_array[ADC_Filter_Pot])};
};

