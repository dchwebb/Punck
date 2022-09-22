#include <Toms.h>
#include <cmath>
#include "Filter.h"
#include "VoiceManager.h"


void Toms::Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, float velocity)
{
	// Called when accessed from MIDI (different note offsets for different tuning?)
	playing = true;
	phase = Phase::Ramp;
	position = 0.0f;
	currentLevel = 0.0f;
	noteMapper->led.On();
	velocityScale = velocity;
	pitchScale = 1.0f + 1.5f * (float)noteOffset / (noteRange == 0 ? 128 : noteRange);
	sineInc = FreqToInc(config.initSineFreq) * pitchScale;
	sineInc2 = config.sineFreq2scale * sineInc;
	sineLevel2 = config.sineLevel2;
	sineLevel = 1.0f;
	position = 2.0f;
	position2 = 2.0f;
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
		sineInc *= config.sineSlowDownRate;			// Sine wave slowly decreases in frequency
		sineInc2 *= config.sineSlowDownRate;			// Sine wave slowly decreases in frequency
		position += sineInc;							// Set current poition in sine wave
		position2 += sineInc2;							// Set current poition in sine wave
		//float decaySpeed = 0.9994f + 0.00055f * static_cast<float>(ADC_array[ADC_TomsDecay]) / 65536.0f;
		sineLevel = sineLevel * config.decaySpeed;
		sineLevel2 = sineLevel2 * config.decaySpeed2;
		currentLevel = std::sin(position) * sineLevel + std::sin(position2) * sineLevel2;

		if (sineLevel <= 0.00001f) {
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
		//outputLevel[0] = velocityScale * filter.CalcFilter(currentLevel, left);
		outputLevel[0] = velocityScale * currentLevel;
		outputLevel[1] = outputLevel[0];
	}
}


void Toms::UpdateFilter()
{
	//filter.Update();
}


uint32_t Toms::SerialiseConfig(uint8_t** buff, uint8_t voiceIndex)
{
	*buff = (uint8_t*)&config;
	//memcpy(buff, &config, sizeof(config));
	return sizeof(config);
}


void Toms::StoreConfig(uint8_t* buff, uint32_t len)
{
	if (len <= sizeof(config)) {
		memcpy(&config, buff, len);
	}
}

