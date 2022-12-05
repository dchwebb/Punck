#pragma once

#include "initialisation.h"




struct PCG {
	struct pcg32_random_t {
		uint64_t state = 0;
		uint64_t inc = seed();

		constexpr uint64_t seed() {
			uint64_t shifted = 0;
			for (const auto c : __TIME__ ) {
				shifted <<= 8;
				shifted |= c;
			}
			return shifted;
		}
	} rng;

	constexpr float get_random(int count) {
	  do {
		  pcg32_random_r();
		--count;
	  } while (count > 0);
	  return pcg32_random_r() / std::numeric_limits<uint32_t>::max();
	}

private:
	constexpr float pcg32_random_r()	{
		uint64_t oldstate = rng.state;
		rng.state = oldstate * 6364136223846793005ULL + (rng.inc | 1);		// Advance internal state
		uint32_t xorshifted = ((oldstate >> 18U) ^ oldstate) >> 27U;		// Calculate output function (XSH RR), uses old state for max ILP
		uint32_t rot = oldstate >> 59U;
		return static_cast<float>((xorshifted >> rot) | (xorshifted << ((-rot) & 31)));
	}

};


// constexpr function to return an array of pseudo random numbers
template <uint32_t... Is>
constexpr auto getRandArray(std::integer_sequence<uint32_t, Is...> a) {
	PCG pcg;
	std::array<float, a.size()> vals{};
	((vals[Is] = pcg.get_random(Is)), ...);
	return vals;
}


// constexpr function to return an array of incrementing integers
template <uint32_t... Is>
constexpr auto getIntArray(std::integer_sequence<uint32_t, Is...> a) {
//	PCG pcg;
	std::array<uint32_t, a.size()> vals{};
    ((vals[Is] = Is), ...);
    return vals;
}


template<uint32_t channels = 8>
constexpr std::array<uint32_t, channels> getDelays()
{
	std::array<uint32_t, channels>delayLengths;

	constexpr float delayMsRange = 50;
	constexpr float delaySamplesRange = delayMsRange * 0.001 * systemSampleRate;		// 2400 at 48kHz

	auto is = getIntArray(std::make_integer_sequence<uint32_t, channels>{});					// constexpr array of incrementing integers
	auto rs = getRandArray(std::make_integer_sequence<uint32_t, channels>{});				// constexpr array of random floats from 0-1

	// Generate array of randomised delay lengths distributed semi-evenly across the diffusion time
	for (auto i : is) {
		float rangeLow = delaySamplesRange * i / channels;
		float rangeHigh = delaySamplesRange * (i + 1) / channels;
		delayLengths[i] = std::max(static_cast<uint32_t>(rangeLow + rs[i] * (rangeHigh - rangeLow)), 10UL);
	}

	return delayLengths;
}


// Use like `Hadamard<float, 8>::inPlace(data)` - size must be a power of 2
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

	static constexpr std::array<uint32_t, channels> delayLen = getDelays();			// Stores randomised delay lengths for each channel
	float delays0[delayLen[0]];
	float delays1[delayLen[1]];
	float delays2[delayLen[2]];
	float delays3[delayLen[3]];
	float delays4[delayLen[4]];
	float delays5[delayLen[5]];
	float delays6[delayLen[6]];
	float delays7[delayLen[7]];


	struct Delays {
		float* delay;
		size_t size;
		uint32_t writePos;

		void write(float sample) {
			delay[writePos++] = sample;
			if (writePos == size) writePos = 0;
		}
	} delays[channels] = {{delays0, delayLen[0], 0},
			  {delays1, delayLen[1], 0},
			  {delays2, delayLen[2], 0},
			  {delays3, delayLen[3], 0},
			  {delays4, delayLen[4], 0},
			  {delays5, delayLen[5], 0},
			  {delays6, delayLen[6], 0},
			  {delays7, delayLen[7], 0}};

	std::array<bool, channels> flipPolarity{false, true, true, false, true, false, true, false};

public:
	std::array<float, channels> Process(std::array<float, channels> input) {
		// Delay
		std::array<float, channels> delayed;
		for (int c = 0; c < channels; ++c) {
			delays[c].write(input[c]);
			delayed[c] = delays[c].delay[delayLen[c]];
		}

		// Mix with a Hadamard matrix
		std::array<float, channels> mixed = delayed;
		Hadamard<float, channels>::inPlace(mixed.data());

		// Flip some polarities
		for (int c = 0; c < channels; ++c) {
			if (flipPolarity[c]) {
				mixed[c] *= -1;
			}
		}

		return mixed;
	}


};

class Reverb {

	DiffuserStep<8> diffuserStep;
public:
	std::tuple<float, float> Process(float left, float right) {
		std::array<float, 8> samples {left, left, left, left, right, right, right, right};
		std::array<float, 8> output = diffuserStep.Process(samples);
		std::tuple<float, float> ret = {(output[0] + output[1] + output[2] + output[3]) / 4, (output[4] + output[5] + output[6] + output[7]) / 4};
		return ret;
	}
};


extern Reverb reverb;
