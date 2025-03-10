#pragma once

#include "initialisation.h"
#include "DrumVoice.h"
#include <string>
#include <array>

class NoteMapper;

class Samples : public DrumVoice {
public:
	enum SamplePlayer {playerA, playerB, noPlayer};

	struct Sample {
		char name[11];
		uint32_t size;						// Size of file in bytes
		uint32_t cluster;					// Starting cluster
		uint32_t lastCluster;				// If file spans multiple clusters store last cluster before jump - if 0xFFFFFFFF then clusters are contiguous
		const uint8_t* startAddr;			// Address of data section
		const uint8_t* endAddr;				// End Address of data section
		uint32_t dataSize;					// Size of data section in bytes
		uint32_t sampleCount;				// Number of samples (stereo samples only counted once)
		uint32_t sampleRate;
		uint8_t byteDepth;
		uint16_t dataFormat;				// 1 = PCM; 3 = Float
		uint8_t channels;					// 1 = mono, 2 = stereo
		SamplePlayer bank;					// Bank A or B (indicated by sample name eg A1xxx.wav or B2xx.wav)
		uint8_t bankIndex;					// The index of the sample in the bank
		bool valid;							// false if header cannot be processed
		float volume;
	} sampleList[128];

	struct Bank {
		Sample* s;
		uint32_t index;
	};

	struct Sampler {
		bool playing = false;
		Sample* sample;
		const uint8_t* sampleAddress;
		float playbackSpeed;				// Multiplier to allow faster or slow playback (and compensate for non 48k samples)
		float fractionalPosition;			// When playing sample at varying rate store how far through the current sample playback is
		int32_t currentSamples[2] = {};		// Left/right sample levels for mixing
		uint32_t bankLen;					// Number of samples in bank
		std::array<Bank, 40> bank;			// Store pointer to Bank samples sorted by index
		NoteMapper* noteMapper;
		float velocityScale;
		volatile uint16_t* voiceADC;
		volatile uint16_t* tuningADC;
		volatile uint16_t* levelADC;
	} sampler[2];

	Samples();
	void Play(const uint8_t player, const uint32_t noteOffset, uint32_t noteRange, const float velocity);
	void Play(const uint8_t player, const uint32_t sampleNo);
	void CalcOutput();
	bool UpdateSampleList();
	uint32_t SerialiseSampleNames(uint8_t** buff, const uint8_t voiceIndex);
	uint32_t SerialiseConfig(uint8_t** buff, const uint8_t voiceIndex);
	void StoreConfig(uint8_t* buff, const uint32_t len);
	uint32_t ConfigSize();

private:
	char longFileName[100];
	uint8_t lfnPosition = 0;
	bool GetSampleInfo(Sample* sample);
	int32_t ParseInt(const std::string_view cmd, const std::string_view precedingChar, const int32_t low = 0, const int32_t high = 0);
};

