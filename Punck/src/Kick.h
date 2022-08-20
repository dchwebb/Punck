#pragma once
#include "initialisation.h"
#include "NoteHandler.h"

class Kick {
public:
	enum class Phase {Off, Ramp1, Ramp2, Ramp3, FastSine, SlowSine} phase;
	float position;

	float ramp1Inc = 0.22f;				// Initial steep ramp
	float ramp2Inc = 0.015f;			// Second slower ramp
	float ramp3Inc = 0.3f;				// Steep Discontinuity leading to fast sine
	float fastSinInc = 0.017f;			// Higher frequency of fast sine section
	float initSlowSinInc = 0.0065f;		// Initial frequency of gradually decreasing slow sine wave
	float sineSlowDownRate = 0.999992f;	// Rate at which slow sine wave frequency decreases

	float slowSinInc;
	float slowSinLevel;
	float ouputLevel;					// After filter applied
	NoteHandler::Voice noteHandlerVoice;

	Kick();
	void Play(uint32_t noteOffset, uint32_t noteRange);
	void CalcSamples();

private:
	float currentLevel;
};

extern Kick kickPlayer;
