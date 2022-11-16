#pragma once

#include "initialisation.h"

// Abstract Class to define interface for drum voices

class DrumVoice {
public:
	float outputLevel[2];
	bool playing;

	virtual void Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, float velocity) = 0;
	virtual void Play(const uint8_t voice, const uint32_t index) = 0;
	virtual void CalcOutput() = 0;
	virtual uint32_t SerialiseConfig(uint8_t** buff, const uint8_t voiceIndex) = 0;		// Return a pointer to config data for saving or transmission over SysEx
	virtual void StoreConfig(uint8_t* buff, const uint32_t len) = 0;					// Reads config data back into member values
	virtual uint32_t ConfigSize() = 0;
	virtual void UpdateFilter() {};

	volatile bool noteQueued;			// Declared volatile to ensure that bool is only set once values are stored ready for playing
	struct QueuedNote {
		uint8_t voice;
		uint32_t noteOffset;
		uint32_t noteRange;
		float velocity;
	} queuedNote;
	void QueuePlay(const uint8_t voice, const uint32_t offset, const uint32_t range, const float velocity) {
		noteQueued = false;
		queuedNote.voice = voice;
		queuedNote.noteOffset = offset;
		queuedNote.noteRange = range;
		queuedNote.velocity = velocity;
		noteQueued = true;
	}

	constexpr float FreqToInc(const float frequency)
	{
		return frequency * (2 * pi) / systemSampleRate;
	}
};

