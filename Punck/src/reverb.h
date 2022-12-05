/* Based on original code by Geraint Luff
 * https://signalsmith-audio.co.uk/writing/2021/lets-write-a-reverb/
 */
#pragma once

#include "initialisation.h"
#include <cstring>

// Usage: `Hadamard<float, 8>::inPlace(data)` - size must be a power of 2
template<typename Sample, int size>
class Hadamard {
public:
	static inline void recursiveUnscaled(Sample* data) {
		if (size <= 1) {
			return;
		}
		constexpr uint32_t hSize = size / 2;

		// Two (unscaled) Hadamards of half the size
		Hadamard<Sample, hSize>::recursiveUnscaled(data);
		Hadamard<Sample, hSize>::recursiveUnscaled(data + hSize);

		// Combine the two halves using sum/difference
		for (uint32_t i = 0; i < hSize; ++i) {
			float a = data[i];
			float b = data[i + hSize];
			data[i] = a + b;
			data[i + hSize] = a - b;
		}
	}

	static inline void inPlace(Sample* data) {
		recursiveUnscaled(data);

		Sample scalingFactor = std::sqrt(1.0 / size);
		for (uint32_t c = 0; c < size; ++c) {
			data[c] *= scalingFactor;
		}
	}
};


template<int channels = 8>
class DiffuserStep {
public:

	DiffuserStep() {
		// Clear delay line buffer
		memset(delayBuffer, 0, sizeof(delayBuffer));
		delays[0].delay = delayBuffer;

		// Generate array of randomised delay lengths distributed semi-evenly across the diffusion time
		for (uint32_t i = 0; i < channels; ++i) {
			const float rangeLow = delaySamplesRange * i / channels;
			const float rangeHigh = delaySamplesRange * (i + 1) / channels;
			delays[i].size = std::max(static_cast<uint32_t>(rangeLow +  (rand() / double(RAND_MAX)) * (rangeHigh - rangeLow)), 10UL);

			// Set the start pointer to the start of the current delay line within the combined delay buffer
			if (i < channels) {
				delays[i + 1].delay = delays[i].delay + delays[i].size;
			}
			delays[i].writePos = 0;

			flipPolarity[i] = (rand() & 1);
		}
	}


	std::array<float, channels> Process(std::array<float, channels> input)
	{
		std::array<float, channels> output;
		for (int c = 0; c < channels; ++c) {
			delays[c].write(input[c]);							// Write current sample into delay line
			output[c] = delays[c].delay[delays[c].readPos];		// Read out oldest delayed sample
		}

		Hadamard<float, channels>::inPlace(output.data());		// Mix with a Hadamard matrix

		for (int c = 0; c < channels; ++c) {					// Flip polarities according to randomised table
			if (flipPolarity[c]) {
				output[c] *= -1;
			}
		}
		return output;
	}

private:
	static constexpr float delayMsRange = 50;
	static constexpr float delaySamplesRange = delayMsRange * 0.001 * systemSampleRate;		// 2400 at 48kHz

	// Create a buffer to hold delay samples - the length of each delay line randomised but grouped in ascending sizes
	// For 8 channels with a delay range of 50ms this means each delay line will a random number partitioned thusly:
	// | 0 - 300 | 300 - 600 | 600 - 900 | 900 - 1200 | 1200 - 1500 | 1500 - 1800 | 1800 - 2100 | 2100 - 2400 |
	float delayBuffer[static_cast<uint32_t>(delaySamplesRange * (channels + 1) / 2)];

	// Hold randomised list of channels to flip polarity of when mixing
	std::array<bool, channels> flipPolarity;

	struct Delays {
		float* delay;							// pointer to location of samples in delayBuffer
		size_t size;							// size of delay buffer
		uint32_t writePos;						// current sample write position
		uint32_t readPos;						// current sample read position

		void write(float sample) {
			delay[writePos++] = sample;
			if (writePos == size) writePos = 0;
			readPos = writePos + 1;				// delay length is constant so always one past the write position
			if (readPos == size) readPos = 0;
		}
	} delays[channels];


};

class Reverb {
public:
	std::pair<float, float> Process(float left, float right) {
		std::array<float, delayChannels> samples {left, left, left, left, right, right, right, right};
		for (auto& d : diffuserStep) {
			samples = d.Process(samples);
		}
		std::pair<float, float> ret = {(samples[0] + samples[1] + samples[2] + samples[3]) / 4, (samples[4] + samples[5] + samples[6] + samples[7]) / 4};
		return ret;
	}

private:
	static constexpr uint32_t diffusionSteps = 3;
	static constexpr uint32_t delayChannels = 8;
	DiffuserStep<delayChannels> diffuserStep[diffusionSteps];
};


extern Reverb reverb;
