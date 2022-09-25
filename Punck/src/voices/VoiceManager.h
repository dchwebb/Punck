#pragma once

#include "initialisation.h"
#include "MidiHandler.h"
#include "Samples.h"
#include "Kick.h"
#include "Snare.h"
#include "HiHat.h"
#include "Toms.h"
#include <cstring>

struct NoteMapper {

	DrumVoice* drumVoice = nullptr;
	uint8_t voice;
	uint8_t voiceIndex = 0;				// For use with players that have multiple outputs (eg sampler)
	uint8_t midiLow;
	uint8_t midiHigh;

	struct Btn {
		GPIO_TypeDef* gpioBank;
		uint8_t gpioPin;
		uint8_t debounce = 0;
		bool buttonOn;

		bool IsOn()  {
			if (gpioBank && (gpioBank->IDR & (1 << gpioPin)) == 0) {
				if (debounce == 0) {
					debounce = 20;
					return true;
				}
				debounce = 20;
			}
			if (debounce > 0) {
				--debounce;
			}
			return false;
		}
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
	enum Voice {kick, snare, hihat, toms, samplerA, samplerB, count};
	//static constexpr uint8_t voiceCount = 5;

	VoiceManager();
	void VoiceLED(Voice v, bool on);
	void NoteOn(MidiHandler::MidiNote midiNote);
	void Output();
	void CheckButtons();
	void IdleTasks();

	Kick kickPlayer;
	Snare snarePlayer;
	Samples samples;
	HiHat hihatPlayer;
	Toms tomsPlayer;

	NoteMapper noteMapper[Voice::count];
private:
	float FastTanh(float x);

	enum class ButtonMode {playNote, midiLearn, drumPattern};
	enum class MidiLearnState {off, lowNote, highNote};
	ButtonMode buttonMode;
	uint8_t midiLearnVoice;
	MidiLearnState midiLearnState;
};

extern VoiceManager voiceManager;
