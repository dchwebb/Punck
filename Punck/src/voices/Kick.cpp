#include <Kick.h>
#include <cmath>
#include "Filter.h"
#include "VoiceManager.h"


void Kick::Play(const uint8_t voice, const uint32_t noteOffset, const uint32_t noteRange, const float velocity)
{
	// Called when accessed from MIDI
	playing = true;
	phase = Phase::Ramp1;
	position = 0.0f;
	currentLevel = 0.0f;
	velocityScale = velocity * (static_cast<float>(ADC_array[ADC_KickLevel]) / 32768.0f);
	fastSinInc = FreqToInc(config.fastSinFreq);
	slowSinInc = FreqToInc(config.initSlowSinFreq);
	slowSinLevel = 1.0f;
}


void Kick::Play(const uint8_t voice, const uint32_t index)
{
	// Called when button is pressed
	Play(0, 0, 0, 1.0f);
}


void Kick::CalcOutput()
{
	switch (phase) {
	case Phase::Ramp1:
		currentLevel = currentLevel + (config.ramp1Inc * (1.0f - currentLevel));

		if (currentLevel > 0.6f) {
			phase = Phase::Ramp2;
		}
		break;

	case Phase::Ramp2:
		currentLevel = currentLevel + (config.ramp2Inc * (1.0f - currentLevel));

		if (currentLevel > 0.82f) {
			currentLevel = 0.78f;				// discontinuity sharply falls at first
			phase = Phase::Ramp3;
		}
		break;

	case Phase::Ramp3:
		currentLevel = currentLevel + (config.ramp3Inc * (1.0f - currentLevel));

		if (currentLevel > 0.93f) {
			position = 2.0f;
			phase = Phase::FastSine;

		}
		break;

	case Phase::FastSine:
		position += fastSinInc;
		currentLevel = std::sin(position);

		if (position >= 1.5f * pi) {
			phase = Phase::SlowSine;
		}
		break;

	case Phase::SlowSine:
	{
		slowSinInc *= config.sineSlowDownRate;			// Sine wave slowly decreases in frequency
		position += slowSinInc;							// Set current poition in sine wave
		const float decaySpeed = 0.9994f + 0.00055f * static_cast<float>(ADC_array[ADC_KickDecay]) / 65536.0f;
		slowSinLevel = slowSinLevel * decaySpeed;
		currentLevel = std::sin(position) * slowSinLevel;

		if (slowSinLevel <= 0.00001f) {
			playing = false;
			phase = Phase::Off;
		}
		break;
	}

	default:
		break;
	}

	if (phase != Phase::Off) {
		noteMapper->pwmLed.Level(slowSinLevel);
		outputLevel[0] = velocityScale * filter.CalcFilter(currentLevel, left);
		outputLevel[1] = outputLevel[0];
	}
}


void Kick::UpdateFilter()
{
	filter.Update();
}


uint32_t Kick::SerialiseConfig(uint8_t** buff, const uint8_t voiceIndex)
{
	*buff = reinterpret_cast<uint8_t*>(&config);
	return sizeof(config);
}


void Kick::StoreConfig(uint8_t* buff, const uint32_t len)
{
	if (len <= sizeof(config)) {
		memcpy(&config, buff, len);
	}
}

uint32_t Kick::ConfigSize()
{
	return sizeof(config);
}
