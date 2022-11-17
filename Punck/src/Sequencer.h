#pragma once

#include "initialisation.h"
#include "VoiceManager.h"

static constexpr uint32_t maxBeatsPerBar = 24;
static constexpr uint8_t getActiveSequence = 127;		// Used in the web editor to request the currently playing sequence (versus a specific one)

class Sequencer {
public:
	struct SeqInfo {
		uint8_t bars = 1;
		uint8_t beatsPerBar = 16;
	};
	bool playing;
	uint8_t activeSequence;
	uint8_t currentBar;
	uint8_t currentBeat;
	bool clockEveryTick = false;		// If false tempo out will only happen on quarter notes

	Sequencer();
	void StartStop(uint8_t sequence);
	void Play();
	SeqInfo GetSeqInfo(uint8_t seq);

	uint32_t GetBar(uint8_t** buff, uint8_t seq, uint8_t bar);
	uint32_t GetSequences(uint8_t** buff);
	void StoreSequences(uint8_t* buff);
	void StoreConfig(uint8_t* buff, uint32_t len, uint8_t seq, uint8_t bar, uint8_t beatsPerBar, uint8_t bars);
	uint32_t SequencesSize();

private:
	struct Sequence {
		SeqInfo info;

		struct Bar {
			struct Beat {
				uint8_t level;		// Volume of note (0-127)
				uint8_t index;		// Note index (eg for voices with multiple channels like the sampler)

			} beat[maxBeatsPerBar][VoiceManager::Voice::count];
		} bar[4];
	} sequence[5];

	float tempo;
	uint32_t position;
	static const uint32_t currSeqBrightness = 50;	// brightness of led indicating playing sequence
	uint32_t clockOn;				// Time when tempo clock is turned on to schedule switching off
	void ChangeSequence(uint8_t newSeq);
};

extern Sequencer sequencer;
