#pragma once
#include "initialisation.h"
#include "MidiHandler.h"


class NoteHandler {
	friend class CDCHandler;
public:
	NoteHandler();
	void NoteOn(MidiHandler::MidiNote midiNote);
	void CheckButtons();
private:
	enum Voice {kick, snare, hatClosed, hatOpen, tomHigh, tomMedium, tomLow, sampler1, sampler2};
	enum class ButtonMode {playNote, midiLearn, drumPattern};
	enum class MidiLearnState {off, lowNote, highNote};
	ButtonMode buttonMode;
	Voice midiLearnVoice;
	MidiLearnState midiLearnState;

	struct NoteMapper {
		Voice voice;
		uint8_t midiLow;
		uint8_t midiHigh;

		bool buttonOn;
		GPIO_TypeDef* gpioBankBtn;
		uint8_t gpioPinBtn;

		GPIO_TypeDef* gpioBankLED;
		uint8_t gpioPinLED;
	} noteMapper[9];


};

extern NoteHandler noteHandler;
