#pragma once

#include "initialisation.h"
#include "VoiceManager.h"

static constexpr uint32_t beatsPerBar = 8;

struct Beat {
	uint8_t level;		// Volume of note (0-127)
	uint8_t index;		// Note index (eg for voices with multiple channels like the sampler)
};

struct Bar {
	Beat beat[beatsPerBar][VoiceManager::voiceCount];
};

class Sequencer {
public:
	Sequencer();
	void Start();
	void Play();

	Bar bar;
	bool playing;
	uint32_t position;
	uint32_t currentBeat;
	uint32_t beatLen;

};

extern Sequencer sequencer;
