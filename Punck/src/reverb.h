#pragma once

#include "initialisation.h"


template<int channels = 8>
constexpr std::array<uint32_t, channels> getDelays() {
	// These should be random but botching for now as do not have a constexpr random number generator
	std::array<uint32_t, channels>returnValues = {250, 456, 687, 1230, 1337, 1670, 1999, 2354};

/*	constexpr float delayMsRange = 50;
	constexpr float delaySamplesRange = delayMsRange * 0.001 * systemSampleRate;		// 2400
	for (constexpr uint32_t i = 0; i < channels; ++i) {
		constexpr float rangeLow = delaySamplesRange * i / channels;
		constexpr float rangeHigh = delaySamplesRange * (i + 1) / channels;
		returnValues[i] = rangeLow + rand() / double(RAND_MAX) * (rangeHigh - rangeLow);
	}
	*/
	return returnValues;
}


template<int channels = 8>
class DiffuserStep {
	//static constexpr rangeLow = delaySamplesRange *
	static constexpr std::array<uint32_t, channels>delaySamples = getDelays();			// Stores randomised delay lengths for each channel
	float delays0[delaySamples[0]];
	float delays1[delaySamples[1]];
	float delays2[delaySamples[2]];
	float delays3[delaySamples[3]];
	float delays4[delaySamples[4]];
	float delays5[delaySamples[5]];
	float delays6[delaySamples[6]];
	float delays7[delaySamples[7]];

	//std::array<float*, channels> delays = {delays0, delays1, delays2, delays3, delays4, delays5, delays6, delays7};

	struct Delays {
		float* delay;
		size_t size;
		uint32_t writePos;

		void write(float sample) {
			delay[writePos++] = sample;
			if (writePos == size) writePos = 0;
		}
	} delays[channels] = {{delays0, delaySamples[0], 0},
			  {delays1, delaySamples[1], 0},
			  {delays2, delaySamples[2], 0},
			  {delays3, delaySamples[3], 0},
			  {delays4, delaySamples[4], 0},
			  {delays5, delaySamples[5], 0},
			  {delays6, delaySamples[6], 0},
			  {delays7, delaySamples[7], 0}};

	std::array<bool, channels> flipPolarity{false, true, true, false, true, false, true, false};

	std::array<float, channels> Process(std::array<float, channels> input) {
		// Delay
		std::array<float, channels> delayed;
		for (int c = 0; c < channels; ++c) {
			delays[c].write(input[c]);
			delayed[c] = delays[c].read(delaySamples[c]);
		}

		// Mix with a Hadamard matrix
		std::array<float, channels> mixed = delayed;
		Hadamard<double, channels>::inPlace(mixed.data());

		// Flip some polarities
		for (int c = 0; c < channels; ++c) {
			if (flipPolarity[c]) mixed[c] *= -1;
		}

		return mixed;
	}


};

class Reverb {
	DiffuserStep<8> diffuserStep;
};
