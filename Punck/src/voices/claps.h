#pragma once
#include "initialisation.h"
#include "DrumVoice.h"
#include "BPFilter.h"

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

	enum class State {start, hit1, hit2, hit3, hit4};
	State state;
	uint32_t stateCounter;

	struct Config {
		float initLevel = 5.0f;
		float initDecay = 0.9930f;				// Decay rate of first hits
		float reverbDecay = 0.9990f;		// Decay rate of 'reverb' section
		float filterCutoff = 1200.0f;
		float filterQ = 2.0f;
		float unfilteredNoiseLevel = 0.05f;
	} config;


	BPFilter filter;		// Band pass filter
};

