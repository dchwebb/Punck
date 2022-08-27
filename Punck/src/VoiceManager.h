#pragma once

#include "initialisation.h"
#include "MidiHandler.h"
#include "samples.h"
#include "Kick.h"
#include "Snare.h"


struct NoteMapper {
	uint8_t voice;
	uint8_t midiLow;
	uint8_t midiHigh;

	DrumVoice* drumVoice = nullptr;
	uint8_t voiceIndex = 0;				// For use with players that have multiple outputs (eg sampler)

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
};


class VoiceManager {
	friend class CDCHandler;
public:
	enum Voice {kick, snare, hatClosed, hatOpen, tomHigh, tomMedium, tomLow, samplerA, samplerB};
	static constexpr uint8_t voiceCount = 9;

	VoiceManager();
	void VoiceLED(Voice v, bool on);
	void NoteOn(MidiHandler::MidiNote midiNote);
	void Output();
	void CheckButtons();
	void IdleTasks();

	Kick kickPlayer;
	Snare snarePlayer;
	Samples samples;

	NoteMapper noteMapper[voiceCount];
private:
	enum class ButtonMode {playNote, midiLearn, drumPattern};
	enum class MidiLearnState {off, lowNote, highNote};
	ButtonMode buttonMode;
	uint8_t midiLearnVoice;
	MidiLearnState midiLearnState;
};

extern VoiceManager voiceManager;
