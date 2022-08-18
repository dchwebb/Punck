#pragma once
#include "initialisation.h"
#include "NoteHandler.h"

class Kick {
public:
	enum class Phase {Off, Ramp1, Ramp2, Ramp3, FastSine, SlowSine} phase;
	float position;
	float fastSinInc;
	float slowSinInc;
	float slowSinLevel;
	float currentLevel;
	NoteHandler::Voice noteHandlerVoice;

	Kick();
	void Play(uint32_t noteOffset, uint32_t noteRange);
	void CalcSamples();

};

extern Kick kickPlayer;
