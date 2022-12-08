#pragma once
#include "initialisation.h"
#include "Filter.h"
#include "BPFilter.h"
#include "DrumVoice.h"

class NoteMapper;
class HiHat  : public DrumVoice {
public:
	void Play(const uint8_t voice, const uint32_t noteOffset, uint32_t noteRange, const float velocity);
	void Play(const uint8_t voice, const uint32_t index);
	void CalcOutput();
	void UpdateFilter() override;
	uint32_t SerialiseConfig(uint8_t** buff, const uint8_t voiceIndex);
	void StoreConfig(uint8_t* buff, const uint32_t len);
	uint32_t ConfigSize();

	NoteMapper* noteMapper;

private:
	struct Config {
		float attackInc = 0.005f;			// Attack envelope speed
		float decay = 0.9991f;				// Decay envelope speed

		float hpInitCutoff = 0.07f;			// HP filter ramps from 1.68kHz to 6.48kHz
		float hpFinalCutoff = 0.27f;
		float hpCutoffInc = 1.01f;

		float lpInitCutoff = 0.5f;
		float lpFinalCutoff = 0.1f;
		float lpCutoffInc = 0.99998f;

		float noiseInitLevel = 0.99f;		// Initial level of noise component
		float noiseDecay = 0.9998f;

		// Relative level and frequency of 6 signal partials
		float partialScale[6] = {0.8f, 0.5f, 0.4f, 0.4f, 0.5f, 0.4f};
		float partialFreq[6] = {569.0f, 621.0f, 1559.0f, 2056.0f, 3300.0f, 5515.0f};

		// Frequency modulation - amount by which partial 0 frequency modulates other partials
		uint8_t partialFM[6] = {0, 8, 1, 6, 1, 0};
	} config;

	Filter<2> hpFilter{filterPass::HighPass, nullptr};
	Filter<2> lpFilter{filterPass::LowPass, nullptr};

	float velocityScale;
	float decayScale;
	bool attack;							// True during the envelope attack phase
	float attackLevel;
	float noiseScale;

	float hpFilterCutoff;
	float lpFilterCutoff;

	// multiplier to convert frequency to half a period of a square wave
	static constexpr uint32_t freqToSqPeriod = systemSampleRate / 2;

	float partialLevel[6];					// Output level of each partial
	uint32_t partialPeriod[6];				// Square wave period
	uint32_t partialPos[6];					// partial position counter

};
