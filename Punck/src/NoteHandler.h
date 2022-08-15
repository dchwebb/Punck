#pragma once
#include "initialisation.h"
#include "MidiHandler.h"


class NoteHandler {
	friend class CDCHandler;
public:
	enum Voice {kick, snare, hatClosed, hatOpen, tomHigh, tomMedium, tomLow, samplerA, samplerB};

	NoteHandler();
	void VoiceLED(Voice v, bool on);
	void NoteOn(MidiHandler::MidiNote midiNote);
	void CheckButtons();
private:
	enum class ButtonMode {playNote, midiLearn, drumPattern};
	enum class MidiLearnState {off, lowNote, highNote};
	ButtonMode buttonMode;
	Voice midiLearnVoice;
	MidiLearnState midiLearnState;

	struct LED {
		GPIO_TypeDef* gpioBank;
		uint8_t gpioPin;

		void On()  { gpioBank->ODR |= (1 << gpioPin); }
		void Off() { gpioBank->ODR &= ~(1 << gpioPin); }
	};

	struct NoteMapper {
		Voice voice;
		uint8_t midiLow;
		uint8_t midiHigh;

		bool buttonOn;
		GPIO_TypeDef* gpioBankBtn;
		uint8_t gpioPinBtn;

		LED led;

	} noteMapper[9];


};

extern NoteHandler noteHandler;
