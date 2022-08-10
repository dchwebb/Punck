#pragma once
#include "initialisation.h"
#include "MidiHandler.h"


class NoteHandler {
public:
	NoteHandler();
	void NoteOn(MidiHandler::MidiNote midiNote);
	void CheckButtons();
private:
	enum Voice {kick, snare, hatClosed, hatOpen, tomHigh, tomMedium, tomLow, sampler1, sampler2};
	enum class ButtonMode {playNote, midiLearn, drumPattern};

	ButtonMode buttonMode;
	Voice midiLearnVoice;

	struct NoteMapper {
		uint8_t midiLow;
		uint8_t midiHigh;
		GPIO_TypeDef* gpioBank;
		uint8_t gpioPin;
		bool buttonOn;
	} noteMapper[9];


};

extern NoteHandler noteHandler;
