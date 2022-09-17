#pragma once

#include "initialisation.h"
#include "VoiceManager.h"

static constexpr uint32_t maxBeatsPerBar = 24;
static constexpr uint8_t getActiveSequence = 127;		// Used in the web editor to request the currently playing sequence (versus a specific one)

class Sequencer {
public:
	struct SeqInfo {
		uint8_t bars;
		uint8_t beatsPerBar;
	};
	bool playing;
	uint8_t activeSequence;
	uint8_t currentBar;
	uint8_t currentBeat;

	Sequencer();
	void StartStop(uint8_t sequence);
	void Play();
	SeqInfo GetSeqInfo(uint8_t seq);

	uint32_t GetBar(uint8_t** buff, uint8_t seq, uint8_t bar);
	void StoreConfig(uint8_t* buff, uint32_t len, uint8_t seq, uint8_t bar, uint8_t beatsPerBar, uint8_t bars);

private:
	struct Sequence {
		SeqInfo info;

		struct Bar {
			struct Beat {
				uint8_t level;		// Volume of note (0-127)
				uint8_t index;		// Note index (eg for voices with multiple channels like the sampler)

			} beat[maxBeatsPerBar][VoiceManager::voiceCount];
		} bar[4];
	} sequence[6];




	float tempo;


	uint32_t position;


};

extern Sequencer sequencer;
