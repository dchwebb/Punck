#pragma once

#include "initialisation.h"

// Store information about samples (file name, format etc)
struct SampleInfo {
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
	uint8_t channels;					// 1 = mono, 2 = stereo
	bool valid;							// false if header cannot be processed
};

class Samples {
public:
	SampleInfo sampleInfo[128];

	bool playing = false;
	uint32_t sampleIndex = 0;
	const uint8_t* sampleAddress;

	int32_t currentSamples[2] = {};

	void Play(uint32_t sampleNo);
	void CalcSamples();
	bool UpdateSampleList();
	bool GetSampleInfo(SampleInfo* sample);
};

extern Samples samples;
