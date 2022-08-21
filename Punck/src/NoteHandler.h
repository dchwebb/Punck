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
	void Output();
	void CheckButtons();
private:
	enum class ButtonMode {playNote, midiLearn, drumPattern};
	enum class MidiLearnState {off, lowNote, highNote};
	ButtonMode buttonMode;
	Voice midiLearnVoice;
	MidiLearnState midiLearnState;

	struct NoteMapper {
		Voice voice;
		uint8_t midiLow;
		uint8_t midiHigh;

		struct Btn {
			GPIO_TypeDef* gpioBank;
			uint8_t gpioPin;
			bool buttonOn;

			bool IsOn()  { return (gpioBank != nullptr) && (gpioBank->IDR & (1 << gpioPin)) == 0; }
		} btn;

		struct LED {
			GPIO_TypeDef* gpioBank;
			uint8_t gpioPin;

			void On()  { gpioBank->ODR |= (1 << gpioPin); }
			void Off() { gpioBank->ODR &= ~(1 << gpioPin); }
		} led;

	} noteMapper[9];


};

extern NoteHandler noteHandler;
