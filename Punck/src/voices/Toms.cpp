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
	pitchScale = 1.0f + (float)noteOffset / (noteRange == 0 ? 128 : noteRange);
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
		currentLevel = currentLevel + (config.ramp1Inc * (1.0f - currentLevel));

		if (currentLevel > 0.93f) {
			position = 2.0f;
			phase = Phase::FastSine;
			GPIOG->ODR |= GPIO_ODR_OD11;		// PG11: debug pin green
		}
		break;

	case Phase::FastSine:
		GPIOG->ODR &= ~GPIO_ODR_OD11;

		position += config.fastSinInc;
		currentLevel = std::sin(position);

		if (position >= 1.5f * pi) {
			slowSinInc = config.initSlowSinInc * pitchScale;
			slowSinLevel = 1.0f;
			phase = Phase::SlowSine;
		}
		break;

	case Phase::SlowSine: {
		slowSinInc *= config.sineSlowDownRate;			// Sine wave slowly decreases in frequency
		position += slowSinInc;					// Set current poition in sine wave
		//float decaySpeed = 0.9994f + 0.00055f * static_cast<float>(ADC_array[ADC_TomsDecay]) / 65536.0f;
		float decaySpeed = 0.9999f;
		slowSinLevel = slowSinLevel * decaySpeed;
		currentLevel = std::sin(position) * slowSinLevel;

		if (slowSinLevel <= 0.00001f) {
			playing = false;
			phase = Phase::Off;
			noteMapper->led.Off();
		}
	}
		break;

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

