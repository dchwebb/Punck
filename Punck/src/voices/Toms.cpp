#include <Toms.h>
#include <cmath>
#include "Filter.h"
#include "VoiceManager.h"


void Toms::Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, float velocity)
{
	// Called when accessed from MIDI (different note offsets for different tuning?)
	playing = true;
	phase = Phase::Ramp;
	noteMapper->led.On();

	currentLevel = 0.0f;
	velocityScale = velocity * (static_cast<float>(ADC_array[ADC_TomsLevel]) / 32768.0f);
	pitchScale = 1.0f + 1.5f * (float)noteOffset / (noteRange == 0 ? 128 : noteRange);

	for (uint8_t i = 0; i < partialCount; ++i) {
		position[i] = 2.0f;
		sineInc[i] = config.sineFreqScale[1] * FreqToInc(config.baseFreq) * pitchScale;
		sineLevel[i] = config.sineInitLevel[i];
	}
}


void Toms::Play(uint8_t voice, uint32_t index)
{
	// Called when button is pressed
	Play(0, 0, 0, 1.0f);
}


void Toms::CalcOutput()
{
	switch (phase) {
	case Phase::Ramp:
		currentLevel = currentLevel + (config.rampInc * (1.0f - currentLevel));

		if (currentLevel > 0.93f) {
			phase = Phase::Sine;
		}
		break;

	case Phase::Sine:
	{
		currentLevel = 0;
		for (uint8_t i = 0; i < partialCount; ++i) {
			sineInc[i] *= config.sineSlowDownRate;			// Sine wave slowly decreases in frequency
			position[i] += sineInc[i];						// Set current poition in sine wave
			sineLevel[i] *= config.decaySpeed[i];
			currentLevel += std::sin(position[i]) * sineLevel[i];
		}

		if (sineLevel[0] <= 0.00001f) {
			playing = false;
			phase = Phase::Off;
			noteMapper->led.Off();
		}
		break;
	}

	default:
		break;
	}

	if (phase != Phase::Off) {
		outputLevel[0] = velocityScale * currentLevel;
		outputLevel[1] = outputLevel[0];
	}
}


void Toms::UpdateFilter()
{

}


uint32_t Toms::SerialiseConfig(uint8_t** buff, uint8_t voiceIndex)
{
	*buff = (uint8_t*)&config;
	return sizeof(config);
}


void Toms::StoreConfig(uint8_t* buff, uint32_t len)
{
	if (len <= sizeof(config)) {
		memcpy(&config, buff, len);
	}
}

