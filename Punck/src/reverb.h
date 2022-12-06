/* Based on original code by Geraint Luff
 * https://signalsmith-audio.co.uk/writing/2021/lets-write-a-reverb/
 */
#pragma once

#include "initialisation.h"
#include <cstring>

extern float reverbMixBuffer[94000];

struct DelayLines {
	float* delay;							// pointer to location of samples in delay buffer
	size_t size;							// size of delay buffer
	uint32_t writePos;						// current sample write position
	uint32_t readPos;						// current sample read position

	void write(float sample) {
		delay[writePos++] = sample;
		if (writePos == size) writePos = 0;
		readPos = writePos + 1;				// delay length is constant so always one past the write position
		if (readPos == size) readPos = 0;
	}
};


// Usage: `Householder<double, 8>::inPlace(data)` - size must be ≥ 1
template<typename Sample, int size>
class Householder {
	static constexpr Sample multiplier{-2.0f / size};
public:
	static void inPlace(Sample *arr) {
		float sum = 0.0f;
		for (uint32_t i = 0; i < size; ++i) {
			sum += arr[i];
		}

		sum *= multiplier;

		for (uint32_t i = 0; i < size; ++i) {
			arr[i] += sum;
		}
	};
};

// Usage: `Hadamard<float, 8>::inPlace(data)` - size must be a power of 2
template<typename Sample, int size>
class Hadamard {
public:
	static inline void RecursiveUnscaled(Sample* data) {
		if (size <= 1) {
			return;
		}
		constexpr uint32_t hSize = size / 2;

		// Two (unscaled) Hadamards of half the size
		Hadamard<Sample, hSize>::RecursiveUnscaled(data);
		Hadamard<Sample, hSize>::RecursiveUnscaled(data + hSize);

		// Combine the two halves using sum/difference
		for (uint32_t i = 0; i < hSize; ++i) {
			float a = data[i];
			float b = data[i + hSize];
			data[i] = a + b;
			data[i + hSize] = a - b;
		}
	}

	static inline void inPlace(Sample* data) {
		RecursiveUnscaled(data);

		Sample scalingFactor = std::sqrt(1.0 / size);
		for (uint32_t c = 0; c < size; ++c) {
			data[c] *= scalingFactor;
		}
	}
};



template<int channels = 8>
class DiffuserStep {
public:

	DiffuserStep()
	{
		memset(delayBuffer, 0, sizeof(delayBuffer));			// Clear delay line buffer
		delays[0].delay = &delayBuffer[0];

		// Generate array of randomised delay lengths distributed semi-evenly across the diffusion time
		for (uint32_t i = 0; i < channels; ++i) {
			const float rangeLow = delaySamplesRange * i / channels;
			const float rangeHigh = delaySamplesRange * (i + 1) / channels;
			delays[i].size = std::max(static_cast<uint32_t>(rangeLow +  (rand() / float(RAND_MAX)) * (rangeHigh - rangeLow)), 10UL);

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
		for (uint32_t c = 0; c < channels; ++c) {
			delays[c].write(input[c]);							// Write current sample into delay line
			output[c] = delays[c].delay[delays[c].readPos];		// Read out oldest delayed sample
		}

		Hadamard<float, channels>::inPlace(output.data());		// Mix with a Hadamard matrix

		for (uint32_t c = 0; c < channels; ++c) {					// Flip polarities according to randomised table
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

	DelayLines delays[channels];
};





template<int channels = 8>
class MixedFeedback {
public:
	using Array = std::array<float, channels>;

	MixedFeedback()
	{
		memset(reverbMixBuffer, 0, sizeof(reverbMixBuffer));		// Clear delay line buffer
		delays[0].delay = reverbMixBuffer;

		// Generate arrays within delay buffer of increasing delay lengths distributed up from baseDelayLength
		for (uint32_t i = 0; i < channels; ++i) {
			float r = static_cast<float>(i) / channels;
			delays[i].size = static_cast<uint32_t>(std::pow(2.0f, r) * baseDelayLength);

			// Set the start pointer to the start of the current delay line within the combined delay buffer
			if (i < channels) {
				delays[i + 1].delay = delays[i].delay + delays[i].size;
			}
			delays[i].writePos = 0;
			delays[i].readPos = 1;
		}
	}

	Array Process(Array input)
	{
		Array output;
		for (uint32_t c = 0; c < channels; ++c) {
			output[c] = delays[c].delay[delays[c].readPos];			// Read out oldest delayed sample
		}

		Householder<float, channels>::inPlace(output.data());		// Mix using a Householder matrix

		for (uint32_t c = 0; c < channels; ++c) {
			float sum = input[c] + output[c] * decayGain;
			delays[c].write(sum);									// Write current sample into delay line
		}
		return output;
	}

private:
	static constexpr float delayMs = 100.0f;
	static constexpr float decayGain = 0.75f;
	static constexpr float baseDelayLength = delayMs * 0.001f * systemSampleRate;		// 150 * .001 * 48000 = 7200

	DelayLines delays[channels];
};



class Reverb {
public:
	std::pair<float, float> Process(float left, float right) {
		std::array<float, delayChannels> samples {left, left, left, left, right, right, right, right};
		for (auto& d : diffuserStep) {
			samples = d.Process(samples);
		}
		samples = feedback.Process(samples);
		std::pair<float, float> ret = {(samples[0] + samples[2] + samples[4] + samples[6]) / 4, (samples[1] + samples[3] + samples[5] + samples[7]) / 4};
		return ret;
	}

private:
	static constexpr uint32_t diffusionSteps = 3;
	static constexpr uint32_t delayChannels = 8;
	DiffuserStep<delayChannels> diffuserStep[diffusionSteps];
	MixedFeedback<8> feedback;
};


extern Reverb reverb;
