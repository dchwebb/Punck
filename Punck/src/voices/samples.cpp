#include <configManager.h>
#include "samples.h"
#include "FatTools.h"
#include "VoiceManager.h"
#include <cstring>
#include <cmath>


Samples::Samples()
{
	sampler[playerA].voiceADC = &ADC_array[ADC_SampleAVoice];
	sampler[playerB].voiceADC = &ADC_array[ADC_SampleBVoice];
	sampler[playerA].tuningADC = &ADC_array[ADC_SampleASpeed];
	sampler[playerB].tuningADC = &ADC_array[ADC_SampleBSpeed];
	sampler[playerA].levelADC = &ADC_array[ADC_SampleALevel];
	sampler[playerB].levelADC = &ADC_array[ADC_SampleBLevel];
}


void Samples::Play(const uint8_t sp, uint32_t noteOffset, const uint32_t noteRange, const float velocity)
{
	if (fatTools.noFileSystem || (sampler[sp].bankLen == 0)) {
		return;
	}
	sampler[sp].playing = true;


	// Get sample from sorted bank list based on player and note offset
	if (noteOffset == sampler[sp].bankLen) {								// Random mode
		noteOffset = RNG->DR % (sampler[sp].bankLen - 1);
	} else {
		noteOffset = (noteOffset < sampler[sp].bankLen) ? noteOffset : 0;	// If no sample at index use first sample in bank
	}
	sampler[sp].sample = sampler[sp].bank[noteOffset].s;
	sampler[sp].sampleAddress = sampler[sp].sample->startAddr;
	sampler[sp].playbackSpeed = static_cast<float>(sampler[sp].sample->sampleRate) / systemSampleRate;
	sampler[sp].velocityScale = sampler[sp].sample->volume * velocity * (static_cast<float>(*sampler[sp].levelADC) / 32768.0f);
}


void Samples::Play(const uint8_t sp, uint32_t index)
{
	// Samples played from button: use voice pot to determine note
	index =  (*sampler[sp].voiceADC * (sampler[sp].bankLen + 1)) / 65536;		// Last position is random mode

	Play(sp, index, 0, 1.0f);
}


static inline int32_t readBytes(const uint8_t* address, const uint8_t bytes, const uint16_t format)
{
	if (!extFlash.memMapMode) {					// If writing to flash attempting to read memory mapped data will hard fault
		return 0;
	}

	// where data size is less than 32 bit, shift left to zero out lower bytes
	switch (bytes) {
	case 1:										// 8 bit data
		return (uint32_t)(*(uint8_t*)address << 24);
	case 2:										// 16 bit data
		return *(uint16_t*)address << 16;
	case 3:										// 24 bit data: Read in 32 bits and shift up 8 bits to make 32 bit value with lower byte zeroed
		return *(uint32_t*)address << 8;
	case 4:
		if (format == 3) {						// 1 = PCM integer; 3 = float
			return (uint32_t)(*(float*)address * floatToIntMult);
		} else {
			return *(uint32_t*)address;			// 32 bit data
		}
	default:
		return 0;
	}
}


void Samples::CalcOutput()
{
	// MIDI notes will be queued from serial/usb interrupts to play in main I2S interrupt loop
	if (noteQueued) {
		Play(queuedNote.voice, queuedNote.noteOffset, queuedNote.noteRange, queuedNote.velocity);
		noteQueued = false;
	}

	playing = (sampler[playerA].playing || sampler[playerB].playing);
	for (auto& sp : sampler) {
		if (sp.playing) {
			auto& bytes = sp.sample->byteDepth;

			sp.currentSamples[left] = readBytes(sp.sampleAddress, bytes, sp.sample->dataFormat);

			if (sp.sample->channels == 2) {
				sp.currentSamples[right] = readBytes(sp.sampleAddress + bytes, bytes, sp.sample->dataFormat);
			} else {
				sp.currentSamples[right] = sp.currentSamples[left];		// Duplicate left channel to right for mono signal
			}

			// Get sample speed from ADC - want range 0.5 - 1.5
			const float adjSpeed = 0.5f + static_cast<float>(*sp.tuningADC) / 65536.0f;

			// Split the next position into an integer jump and fractional position
			float addressJump;			// Integral part of position
			sp.fractionalPosition = std::modf(sp.fractionalPosition + (adjSpeed * sp.playbackSpeed), &addressJump);
			sp.sampleAddress += (sp.sample->channels * bytes * (uint32_t)addressJump);

			if ((uint8_t*)sp.sampleAddress > sp.sample->endAddr) {
				sp.currentSamples[left] = 0;
				sp.currentSamples[right] = 0;
				sp.playing = false;
				sp.noteMapper->pwmLed.Level(0.0f);
			} else {
				// Apply fade out to led based on position in sample
				const float samplePos = (float)((uint32_t)(sp.sampleAddress - sp.sample->startAddr));
				sp.noteMapper->pwmLed.Level(1.0f - samplePos / sp.sample->dataSize);		// FIXME - could precompute 1 / dataSize and use multiply instead of divide
			}

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
	uint32_t pos = 12;											// First chunk ID at 12 byte (4 word) offset
	while (*(uint32_t*)&(wavHeader[pos]) != 0x20746D66) {		// Look for string 'fmt '
		pos += (8 + *(uint32_t*)&(wavHeader[pos + 4]));			// Each chunk title is followed by the size of that chunk which can be used to locate the next one
		if  (pos > 1000) {
			return false;
		}
	}

	sample->dataFormat = *(uint16_t*)&(wavHeader[pos + 8]);
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

		if (dirEntry->attr == FATFileInfo::LONG_NAME) {
			// Store long file name in temporary buffer as this may contain volume and panning information
			const FATLongFilename* lfn = (FATLongFilename*)dirEntry;
			const char tempFileName[13] {lfn->name1[0], lfn->name1[2], lfn->name1[4], lfn->name1[6], lfn->name1[8],
				lfn->name2[0], lfn->name2[2], lfn->name2[4], lfn->name2[6], lfn->name2[8], lfn->name2[10],
				lfn->name3[0], lfn->name3[2]};

			memcpy(&longFileName[((lfn->order & 0x3F) - 1) * 13], tempFileName, 13);		// strip 0x40 marker from first LFN entry order field
			lfnPosition += 13;

		// Valid sample: not LFN, not deleted, not directory, extension = WAV
		} else if (dirEntry->name[0] != 0xE5 && (dirEntry->attr & AM_DIR) == 0 && strncmp(&(dirEntry->name[8]), "WAV", 3) == 0) {
			Sample* sample = &(sampleList[pos++]);

			// If directory entry preceeded by long file name use that to check for volume/panning information
			float newVolume = 1.0f;
			if (lfnPosition > 0) {
				longFileName[lfnPosition] = '\0';
				const int32_t vol = ParseInt(longFileName, ".v", 0, 100);
				if (vol > 0) {
					newVolume = static_cast<float>(vol) / 100.0f;
				}
			}

			// Check if any fields have changed
			if (sample->cluster != dirEntry->firstClusterLow || sample->size != dirEntry->fileSize ||
					strncmp(sample->name, dirEntry->name, 11) != 0 || newVolume != sample->volume) {
				changed = true;
				strncpy(sample->name, dirEntry->name, 11);
				sample->cluster = dirEntry->firstClusterLow;
				sample->size = dirEntry->fileSize;
				sample->bank = (sample->name[0] == 'A') ? playerA : (sample->name[0] == 'B') ? playerB : noPlayer;
				sample->bankIndex = std::strtol(&(sample->name[1]), nullptr, 10);
				sample->valid = GetSampleInfo(sample);
				sample->volume = newVolume;
			}
		} else {
			lfnPosition = 0;
		}
		dirEntry++;
	}

	// Blank next sample (if exists) to show end of list
	Sample* sample = &(sampleList[pos++]);
	sample->name[0] = 0;

	// Update sorted lists of pointers
	const Bank blank = {nullptr, std::numeric_limits<uint32_t>::max()};		// Fill bank arrays with dummy values to enable sorting
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


uint32_t Samples::SerialiseSampleNames(uint8_t** buff, const uint8_t voiceIndex)
{
	// Copy the first 8 characters of each file name to the config buffer
	uint8_t s = 0;
	for (; s < sampler[voiceIndex].bankLen + 1; ++s) {
		// overflow checking
		if ((uint32_t)((s * 8) + 8) >= sizeof(configManager.configBuffer)) {
			break;
		}

		if (s < sampler[voiceIndex].bankLen) {
			const char* sampleName = sampler[voiceIndex].bank[s].s->name;
			memcpy(configManager.configBuffer + (s * 8), sampleName, 8);
		} else {
			// Create dummy name for random mode
			strncpy((char*)configManager.configBuffer + (s * 8), "-Random-", 8);
		}
	}

	// Check that there are no characters with ASCII code > 127 for sysex transmission (replace with "_")
	for (uint8_t i; i < s * 8; ++i) {
		if (configManager.configBuffer[i] & 0x80) {
			configManager.configBuffer[i] = '_';
		}
	}
	*buff = configManager.configBuffer;
	return s * 8;
}


int32_t Samples::ParseInt(const std::string_view cmd, const std::string_view precedingChar, const int32_t low, const int32_t high)
{
	int32_t val = -1;
	const int8_t pos = cmd.find(precedingChar);		// locate position of character preceding
	if (pos >= 0 && std::strspn(&cmd[pos + precedingChar.size()], "0123456789-") > 0) {
		val = std::stoi(&cmd[pos + precedingChar.size()]);
	}
	if (high > low && (val > high || val < low)) {
		printf("Must be a value between %ld and %ld\r\n", low, high);
		return low - 1;
	}
	return val;
}


uint32_t Samples::SerialiseConfig(uint8_t** buff, const uint8_t voiceIndex)
{
	return 0;
}


void Samples::StoreConfig(uint8_t* buff, const uint32_t len)
{
}

uint32_t Samples::ConfigSize()
{
	return 0;
}
