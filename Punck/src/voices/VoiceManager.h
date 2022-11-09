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

	struct Trigger {
		GPIO_TypeDef* btnGpioBank;
		uint8_t btnGpioPin;
		GPIO_TypeDef* trgGpioBank;
		uint8_t trgGpioPin;

		uint8_t debounce = 0;
		bool buttonOn;

		bool IsOn()  {
			if ((btnGpioBank && (btnGpioBank->IDR & (1 << btnGpioPin)) == 0) ||
			    (trgGpioBank && (trgGpioBank->IDR & (1 << trgGpioPin)) == 0)) {
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
	} trigger;

	struct PWMLED {
		volatile uint32_t* timerChannel;

		void Level(float brightness)  { *timerChannel = (brightness * 4095.0f); }
	} pwmLed;
};


class VoiceManager {
	friend class CDCHandler;
public:
	enum Voice {kick, snare, hihat, toms, samplerA, samplerB, count};

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
	float FastTanh(const float x);

	enum class ButtonMode {playNote, midiLearn, drumPattern};
	enum class MidiLearnState {off, lowNote, highNote};
	ButtonMode buttonMode;
	uint8_t midiLearnVoice;
	MidiLearnState midiLearnState;
};

extern VoiceManager voiceManager;
