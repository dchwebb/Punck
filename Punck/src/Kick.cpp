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
		}
	}
		break;

	case Phase::Ramp2: {
		float target = 0.7f;
		float inc = 0.017f;
		currentLevel = currentLevel + (inc * (target - currentLevel));

		if (currentLevel > target * 0.85) {
			phase = Phase::Ramp3;
		}
	}

		break;

	case Phase::Ramp3: {
		float target = 0.7f;
		float inc = 0.2f;
		currentLevel = currentLevel + (inc * (target - currentLevel));

		if (currentLevel > target * 0.93) {
			fastSinInc = 0.018f;
			position = 2.0f;
			phase = Phase::FastSine;
		}
	}
		break;

	case Phase::FastSine:
		position += fastSinInc;
		currentLevel = std::sin(position) * 0.85f;

		if (position >= 1.5f * pi) {
			slowSinInc = 0.0075f;
			slowSinLevel = 0.85f;
			phase = Phase::SlowSine;
		}
		break;

	case Phase::SlowSine: {
		position += slowSinInc;
		float inc = 0.9999f;
		slowSinLevel = slowSinLevel * inc;
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


