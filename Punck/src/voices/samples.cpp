#include "samples.h"
#include "FatTools.h"
#include "VoiceManager.h"
#include "config.h"

#include <cstring>
#include <cmath>


void Samples::Play(uint8_t sp, uint32_t noteOffset, uint32_t noteRange, float velocity)
{
	if (fatTools.noFileSystem) {
		return;
	}
	// Get sample from sorted bank list based on player and note offset
	if (noteOffset < sampler[sp].bankLen) {
		sampler[sp].sample = sampler[sp].bank[noteOffset].s;
	} else {
		sampler[sp].sample = sampler[sp].bank[0].s;
	}

	sampler[sp].playing = true;
	sampler[sp].sampleAddress = sampler[sp].sample->startAddr;
	sampler[sp].playbackSpeed = (float)sampler[sp].sample->sampleRate / systemSampleRate;
	sampler[sp].sampleVoice = noteOffset;
	sampler[sp].velocityScale = velocity;

	sampler[sp].noteMapper->led.On();
}


void Samples::Play(uint8_t sp, uint32_t index)
{
	if (fatTools.noFileSystem) {
		return;
	}
	sampler[sp].playing = true;
	sampler[sp].sample = sampler[sp].bank[index].s;		//&sampleList[index];
	sampler[sp].sampleAddress = sampler[sp].sample->startAddr;
	sampler[sp].playbackSpeed = (float)sampler[sp].sample->sampleRate / systemSampleRate;
	sampler[sp].velocityScale = 1.0f;

	sampler[sp].noteMapper->led.On();

}


static inline int32_t readBytes(const uint8_t* address, uint8_t bytes)
{
	if (bytes == 3) {
		return *(uint32_t*)(address) << 8;		// 24 bit data: Read in 32 bits and shift up 8 bits to make 32 bit value with lower byte zeroed
	} else {
		return *(uint16_t*)(address) << 16;		// assume 16 bit data
	}
}


void Samples::CalcOutput()
{
	playing = (sampler[playerA].playing || sampler[playerB].playing);
	for (auto& sp : sampler) {
		if (sp.playing) {
			auto& bytes = sp.sample->byteDepth;

			sp.currentSamples[left] = readBytes(sp.sampleAddress, bytes);

			if (sp.sample->channels == 2) {
				sp.currentSamples[right] = readBytes(sp.sampleAddress + bytes, bytes);
			} else {
				sp.currentSamples[right] = sp.currentSamples[left];		// Duplicate left channel to right for mono signal
			}

			// Get sample speed from ADC - want range 0.5 - 1.5
			float adjSpeed = 0.5f + (float)ADC_array[ADC_SampleSpeed] / 65536.0f;		// FIXME - separate ADCs for each sampler

			// Split the next position into an integer jump and fractional position
			float addressJump;
			sp.fractionalPosition = std::modf(sp.fractionalPosition + (adjSpeed * sp.playbackSpeed), &addressJump);
			sp.sampleAddress += (sp.sample->channels * bytes * (uint32_t)addressJump);

			if ((uint8_t*)sp.sampleAddress > sp.sample->endAddr) {
				sp.currentSamples[left] = 0;
				sp.currentSamples[right] = 0;
				sp.playing = false;
				sp.noteMapper->led.Off();
			}
		} else {
			sp.currentSamples[left] = 0;
			sp.currentSamples[right] = 0;
		}
	}

	if (playing) {
		// Mix samples for final output to DAC
		outputLevel[left] = intToFloatMult *
				(sampler[playerA].velocityScale * sampler[playerA].currentSamples[left] +
				 sampler[playerB].velocityScale * sampler[playerB].currentSamples[left]);

		outputLevel[right] = intToFloatMult *
				(sampler[playerA].velocityScale * sampler[playerA].currentSamples[right] +
				 sampler[playerB].velocityScale * sampler[playerB].currentSamples[right]);
	}
}


bool Samples::GetSampleInfo(Sample* sample)
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
		if  (pos > 1000) {
			return false;
		}
	}

	sample->sampleRate = *(uint32_t*)&(wavHeader[pos + 12]);
	sample->channels   = *(uint16_t*)&(wavHeader[pos + 10]);
	sample->byteDepth  = *(uint16_t*)&(wavHeader[pos + 22]) / 8;

	// Navigate forward to find the start of the data area
	while (*(uint32_t*)&(wavHeader[pos]) != 0x61746164) {		// Look for string 'data'
		pos += (8 + *(uint32_t*)&(wavHeader[pos + 4]));
		if (pos > 1200) {
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
			Sample* sample = &(sampleList[pos++]);

			// Check if any fields have changed
			if (sample->cluster != dirEntry->firstClusterLow || sample->size != dirEntry->fileSize || strncmp(sample->name, dirEntry->name, 11) != 0) {
				changed = true;
				strncpy(sample->name, dirEntry->name, 11);
				sample->cluster = dirEntry->firstClusterLow;
				sample->size = dirEntry->fileSize;
				sample->bank = (sample->name[0] == 'A') ? playerA : (sample->name[0] == 'B') ? playerB : noPlayer;
				sample->bankIndex = std::strtol(&(sample->name[1]), nullptr, 10);
				sample->valid = GetSampleInfo(sample);
			}
		}
		dirEntry++;
	}

	// Blank next sample (if exists) to show end of list
	Sample* sample = &(sampleList[pos++]);
	sample->name[0] = 0;

	// Update sorted lists of pointers
	Bank blank = {nullptr, std::numeric_limits<uint32_t>::max()};		// Fill bank arrays with dummy values to enable sorting
	std::fill(sampler[playerA].bank.begin(), sampler[playerA].bank.end(), blank);
	std::fill(sampler[playerB].bank.begin(), sampler[playerB].bank.end(), blank);

	sampler[playerA].bankLen = 0;
	sampler[playerB].bankLen = 0;

	for (Sample& s : sampleList) {
		if (s.name[0] == 0) {
			break;
		}
		// Identify which sample bank the sample is associated with and add to respective array for sorting
		if (s.valid && s.bank < noPlayer) {
			Sampler& sp = sampler[s.bank];
			if (sp.bankLen < sp.bank.size()) {
				sp.bank[sp.bankLen].s = &s;
				sp.bank[sp.bankLen++].index = s.bankIndex;
			}
		}
	}

	auto sorter = [](const Bank &a, const Bank &b){return a.index < b.index;};
	std::sort(sampler[playerA].bank.begin(), sampler[playerA].bank.end(), sorter);
	std::sort(sampler[playerB].bank.begin(), sampler[playerB].bank.end(), sorter);

	return changed;
}


uint32_t Samples::SerialiseSampleNames(uint8_t** buff, uint8_t voiceIndex)
{
	// Copy the first 8 characters of each file name to the config buffer
	uint8_t s = 0;
	for (; s < sampler[voiceIndex].bankLen; ++s) {
		// overflow checking
		if ((uint32_t)((s * 8) + 8) >= sizeof(config.configBuffer)) {
			break;
		}

		const char* sampleName = sampler[voiceIndex].bank[s].s->name;
		memcpy(config.configBuffer + (s * 8), sampleName, 8);
	}

	// Check that there are no characters with ASCII code > 127 for sysex transmission (replace with "_")
	for (uint8_t i; i < s * 8; ++i) {
		if (config.configBuffer[i] & 0x80) {
			config.configBuffer[i] = '_';
		}
	}
	*buff = config.configBuffer;
	return s * 8;
}


uint32_t Samples::SerialiseConfig(uint8_t** buff, uint8_t voiceIndex)
{
	return 0;
}


void Samples::StoreConfig(uint8_t* buff, uint32_t len)
{
}
