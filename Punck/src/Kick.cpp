#include <Kick.h>
#include <cmath>

Kick kickPlayer;

Kick::Kick()
{
	// Store note handler voice for managing LEDs etc
	noteHandlerVoice = NoteHandler::kick;
}

void Kick::Play(uint32_t noteOffset, uint32_t noteRange)
{
	phase = Phase::Ramp1;
	position = 0.0f;
	currentLevel = 0.0f;

	noteHandler.VoiceLED(noteHandlerVoice, true);
}


void Kick::CalcSamples()
{
	switch (phase) {
	case Phase::Ramp1: {
		float target = 0.7f;
		float inc = 0.22f;
		currentLevel = currentLevel + (inc * (target - currentLevel));

		if (currentLevel > target * 0.6) {
			phase = Phase::Ramp2;
			GPIOG->ODR |= GPIO_ODR_OD11;		// PG11: debug pin green
		}
	}
		break;

	case Phase::Ramp2: {
		float target = 0.7f;
		float inc = 0.017f;
		currentLevel = currentLevel + (inc * (target - currentLevel));

		if (currentLevel > target * 0.85) {
			phase = Phase::Ramp3;
			GPIOG->ODR &= ~GPIO_ODR_OD11;

		}
	}

		break;

	case Phase::Ramp3: {

		float target = 0.7f;
		float inc = 0.3f;
		currentLevel = currentLevel + (inc * (target - currentLevel));

		if (currentLevel > target * 0.93) {
			fastSinInc = 0.017f;
			position = 2.0f;
			phase = Phase::FastSine;

		}
	}
		break;

	case Phase::FastSine:
		position += fastSinInc;
		currentLevel = std::sin(position) * 0.85f;

		if (position >= 1.5f * pi) {
			slowSinInc = 0.0065f;
			slowSinLevel = 0.85f;
			phase = Phase::SlowSine;
		}
		break;

	case Phase::SlowSine: {
		slowSinInc *= 0.999992;
		position += slowSinInc;
		float decaySpeed = 0.9994 + 0.00055f * (float)ADC_array[ADC_KickDecay] / 65536.0f;
		//float inc = 0.9999f;
		slowSinLevel = slowSinLevel * decaySpeed;
		currentLevel = std::sin(position) * slowSinLevel;

		if (slowSinLevel <= 0.001f) {
			phase = Phase::Off;
			currentLevel = 0.0f;
			noteHandler.VoiceLED(noteHandlerVoice, false);		// Turn off LED
		}
	}
		break;

	default:
		break;
	}
}


