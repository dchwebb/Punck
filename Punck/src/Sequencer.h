#pragma once

#include "initialisation.h"
#include "VoiceManager.h"

static constexpr uint32_t beatsPerBar = 16;


class Sequencer {
public:
	struct SeqInfo {
		uint8_t bars;
		uint8_t beatsPerBar;
	};

	Sequencer();
	void Start(uint8_t sequence);
	void Play();
	SeqInfo GetSeqInfo(uint8_t seq);

	uint32_t SerialiseConfig(uint8_t** buff, uint8_t seq, uint8_t bar);
	void StoreConfig(uint8_t* buff, uint32_t len, uint8_t seq, uint8_t bar);

private:
	struct Sequence {
		SeqInfo info;

		struct Bar {
			struct Beat {
				uint8_t level;		// Volume of note (0-127)
				uint8_t index;		// Note index (eg for voices with multiple channels like the sampler)

			} beat[beatsPerBar][VoiceManager::voiceCount];
		} bar[4];
	} sequence[6];



	bool playing;
	float tempo;

	uint8_t activeSequence;
	uint8_t currentBar;
	uint32_t currentBeat;
	uint32_t position;


};

extern Sequencer sequencer;
