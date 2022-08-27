#include <Kick.h>
#include <cmath>
#include "Filter.h"
#include "VoiceManager.h"


void Kick::Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, float velocity)
{
	// Called when accessed from MIDI (different note offsets for different tuning?)
	phase = Phase::Ramp1;
	position = 0.0f;
	currentLevel = 0.0f;
	noteMapper->led.On();
	velocityScale = velocity;
}


void Kick::Play(uint8_t voice, uint32_t index)
{
	// Called when button is pressed
	phase = Phase::Ramp1;
	position = 0.0f;
	currentLevel = 0.0f;
	noteMapper->led.On();
	velocityScale = 1.0f;
}


void Kick::CalcOutput()
{
	switch (phase) {
	case Phase::Ramp1:
		currentLevel = currentLevel + (ramp1Inc * (1.0f - currentLevel));

		if (currentLevel > 0.6f) {
			phase = Phase::Ramp2;
			GPIOG->ODR |= GPIO_ODR_OD11;		// PG11: debug pin green
		}
		break;

	case Phase::Ramp2:
		currentLevel = currentLevel + (ramp2Inc * (1.0f - currentLevel));

		if (currentLevel > 0.82f) {
			currentLevel = 0.78f;				// discontinuity sharply falls at first
			phase = Phase::Ramp3;
			GPIOG->ODR &= ~GPIO_ODR_OD11;
		}
		break;

	case Phase::Ramp3:
		currentLevel = currentLevel + (ramp3Inc * (1.0f - currentLevel));

		if (currentLevel > 0.93f) {
			position = 2.0f;
			phase = Phase::FastSine;

		}
		break;

	case Phase::FastSine:
		position += fastSinInc;
		currentLevel = std::sin(position);

		if (position >= 1.5f * pi) {
			slowSinInc = initSlowSinInc;
			slowSinLevel = 1.0f;
			phase = Phase::SlowSine;
		}
		break;

	case Phase::SlowSine: {
		slowSinInc *= sineSlowDownRate;			// Sine wave slowly decreases in frequency
		position += slowSinInc;					// Set current poition in sine wave
		float decaySpeed = 0.9994 + 0.00055f * static_cast<float>(ADC_array[ADC_KickDecay]) / 65536.0f;
		slowSinLevel = slowSinLevel * decaySpeed;
		currentLevel = std::sin(position) * slowSinLevel;

		if (slowSinLevel <= 0.00001f) {
			phase = Phase::Off;
			//currentLevel = 0.0f;			// FIXME somthing is causing a discontuinity where output has a non-zero offset

			noteMapper->led.Off();
		}
	}
		break;

	default:
		break;
	}

	outputLevel[0] = velocityScale * filter.CalcFilter(currentLevel, left);
	outputLevel[1] = outputLevel[0];
}


void Kick::UpdateFilter()
{
	filter.Update();
}


uint32_t Kick::SerialiseConfig(uint8_t* buff)
{
	return 0;
}
