#include <samples.h>
#include "FatTools.h"
#include <cstring>
#include <cmath>

Samples samples;

uint32_t startTime;		// Debug

void Samples::Play(uint32_t index, uint32_t noteOffset, uint32_t noteRange)
{
	startTime = SysTickVal;

	playing = true;
	sampleIndex = index;
	sampleAddress = sampleList[index].startAddr;
	playbackSpeed = (float)sampleList[index].sampleRate / systemSampleRate;
	sampleVoice = noteOffset;

	GPIOE->ODR |= GPIO_ODR_OD1;				// PE1: Yellow LED nucleo
}


static inline int32_t readBytes(const uint8_t* address, uint8_t bytes)
{
	if (bytes == 3) {
		return *(uint32_t*)(address) << 8;		// 24 bit data: Read in 32 bits and shift up 8 bits to make 32 bit value with lower byte zeroed
	} else {
		return *(uint16_t*)(address) << 16;		// assume 16 bit data
	}
}


void Samples::CalcSamples()
{
	if (playing) {
		auto& bytes = sampleList[sampleIndex].byteDepth;

		currentSamples[0] = readBytes(sampleAddress, bytes);

		if (sampleList[sampleIndex].channels == 2) {
			currentSamples[1] = readBytes(sampleAddress + bytes, bytes);
		} else {
			currentSamples[1] = currentSamples[0];		// Duplicate left channel to right for mono signal
		}

		// Get sample speed from ADC - want range 0.5 - 1.5
		float adjSpeed = 0.5f + (float)ADC_array[ADC_SampleSpeed] / 65536.0f;

		// Split the next position into an integer jump and fractional position
		float addressJump;
		fractionalPosition = std::modf(fractionalPosition + (adjSpeed * playbackSpeed), &addressJump);
		sampleAddress += (sampleList[sampleIndex].channels * bytes * (uint32_t)addressJump);

		if ((uint8_t*)sampleAddress > sampleList[sampleIndex].endAddr) {
			currentSamples[0] = 0;
			currentSamples[1] = 0;
			playing = false;

			printf("Time: %f\r\n", (float)(SysTickVal - startTime) / 1000.0f);

			GPIOE->ODR &= ~GPIO_ODR_OD1;			// PE1: Yellow LED nucleo
		}
	}
}


bool Samples::GetSampleInfo(SampleInfo* sample)
{
	// populate the sample object with sample rate, number of channels etc
	// Parsing the .wav format is a pain because the header is split into a variable number of chunks and sections are not word aligned

	const uint8_t* wavHeader = fatTools.GetClusterAddr(sample->cluster, true);

	// Check validity
	if (*(uint32_t*)wavHeader != 0x46464952) {					// wav file should start with letters 'RIFF'
		return false;
	}

	// Jump through chunks looking for 'fmt' chunk
	uint32_t pos = 12;				// First chunk ID at 12 byte (4 word) offset
	while (*(uint32_t*)&(wavHeader[pos]) != 0x20746D66) {		// Look for string 'fmt '
		pos += (8 + *(uint32_t*)&(wavHeader[pos + 4]));			// Each chunk title is followed by the size of that chunk which can be used to locate the next one
		if  (pos > 100) {
			return false;
		}
	}

	sample->sampleRate = *(uint32_t*)&(wavHeader[pos + 12]);
	sample->channels = *(uint16_t*)&(wavHeader[pos + 10]);
	sample->byteDepth = *(uint16_t*)&(wavHeader[pos + 22]) / 8;

	// Navigate forward to find the start of the data area
	while (*(uint32_t*)&(wavHeader[pos]) != 0x61746164) {		// Look for string 'data'
		pos += (8 + *(uint32_t*)&(wavHeader[pos + 4]));
		if (pos > 200) {
			return false;
		}
	}

	sample->dataSize = *(uint32_t*)&(wavHeader[pos + 4]);		// Num Samples * Num Channels * Bits per Sample / 8
	sample->sampleCount = sample->dataSize / (sample->channels * sample->byteDepth);
	sample->startAddr = &(wavHeader[pos + 8]);

	// Follow cluster chain and store last cluster if not contiguous to tell playback engine when to do a fresh address lookup
	uint32_t cluster = sample->cluster;
	sample->lastCluster = 0xFFFFFFFF;

	while (fatTools.clusterChain[cluster] != 0xFFFF) {
		if (fatTools.clusterChain[cluster] != cluster + 1 && sample->lastCluster == 0xFFFFFFFF) {		// Store cluster at first discontinuity of chain
			sample->lastCluster = cluster;
		}
		cluster = fatTools.clusterChain[cluster];
	}
	sample->endAddr = fatTools.GetClusterAddr(cluster);
	return true;
}


bool Samples::UpdateSampleList()
{
	// Updates list of samples from FAT root directory
	FATFileInfo* dirEntry = fatTools.rootDirectory;

	uint32_t pos = 0;
	bool changed = false;

	while (dirEntry->name[0] != 0) {
		// Check not LFN, not deleted, not directory, extension = WAV
		if (dirEntry->attr != 0xF && dirEntry->name[0] != 0xE5 && (dirEntry->attr & AM_DIR) == 0 && strncmp(&(dirEntry->name[8]), "WAV", 3) == 0) {
			SampleInfo* sample = &(sampleList[pos++]);

			// Check if any fields have changed
			if (sample->cluster != dirEntry->firstClusterLow || sample->size != dirEntry->fileSize || strncmp(sample->name, dirEntry->name, 11) != 0) {
				changed = true;
				strncpy(sample->name, dirEntry->name, 11);
				sample->cluster = dirEntry->firstClusterLow;
				sample->size = dirEntry->fileSize;

				sample->valid = GetSampleInfo(sample);
			}
		}
		dirEntry++;
	}

	// Blank next sample (if exists) to show end of list
	SampleInfo* sample = &(sampleList[pos++]);
	sample->name[0] = 0;

	return changed;
}
