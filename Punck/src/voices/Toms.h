#pragma once
#include "initialisation.h"
#include "Filter.h"
#include "DrumVoice.h"

class NoteMapper;

class Toms : public DrumVoice {
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
	enum class Phase {Off, Ramp, Sine} phase;
	static constexpr uint8_t partialCount = 2;

	float position[partialCount];
	float currentLevel;
	float velocityScale;

	float sineInc[partialCount];
	float sineLevel[partialCount];
	float pitchScale;						// Note index allows different frequency notes

	struct Config {
		float decaySpeed[partialCount] = {0.9995f, 0.9993f};	// Volume decay speed
		float rampInc = 0.2f;									// Initial wave steep ramp
		float baseFreq = 60.0f;									// Initial frequency of sine wave
		float sineFreqScale[partialCount] = {1.0f, 1.588f};		// Relative frequencies of partials
		float sineInitLevel[partialCount] = {1.0f, 0.0f};		// Initial volume level
		float sineSlowDownRate = 0.99995f;						// Rate of decrease of sine frequency
	} config;

};

