#pragma once

#include "initialisation.h"
#include "MidiHandler.h"
#include "Samples.h"
#include "Kick.h"
#include "Snare.h"
#include "HiHat.h"
#include "Toms.h"
#include "Claps.h"
#include <cstring>

struct NoteMapper {
	enum  TriggerType: uint8_t {noTrigger = 0, triggerBtn = 1, trigger1 = 2, trigger2 = 4};
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
		GPIO_TypeDef* tr2GpioBank;
		uint8_t tr2GpioPin;

		uint8_t debounce = 0;
		bool buttonOn;

		TriggerType IsOn()  {
			TriggerType triggers = (TriggerType)((btnGpioBank && (btnGpioBank->IDR & (1 << btnGpioPin)) == 0) ? 1 : 0 |
							   (trgGpioBank && (trgGpioBank->IDR & (1 << trgGpioPin)) == 0) ? 2 : 0 |
							   (tr2GpioBank && (tr2GpioBank->IDR & (1 << tr2GpioPin)) == 0) ? 4 : 0);
			if (triggers) {		// Fixme - probably need different debounce on each trigger
				if (debounce == 0) {
					debounce = 20;
					return triggers;
				}
				debounce = 20;
			}
			if (debounce > 0) {
				--debounce;
			}
			return noTrigger;
		}
	} trigger;

	struct PWMLED {
		volatile uint32_t* timerChannel;
		uint32_t minLevel = 0;

		uint32_t GetLevel()  {
			return *timerChannel;
		}
		void Level(float brightness)  {
			*timerChannel = std::max(minLevel, static_cast<uint32_t>(brightness * 4095.0f));
		}
		void setMinLevel(uint32_t level) {		// Sets LED current/minimum brightness: to display currently playing sequence with dimly illuminated button
			minLevel = level;
			*timerChannel = level;
		}
	} pwmLed;
};


class VoiceManager {
	friend class CDCHandler;
public:
	enum Voice {kick, snare, hihat, samplerA, samplerB, toms, claps, count};

	VoiceManager();
	void VoiceLED(Voice v, bool on);
	void NoteOn(MidiHandler::MidiNote midiNote);
	void Output();
	void CheckButtons();
	void IdleTasks();
	void SetAllLeds(float val);
	void RestoreAllLeds();


	uint32_t GetConfig(uint8_t** buff);							// Return a pointer to config data for saving
	uint32_t StoreConfig(uint8_t* buff);						// Reads config data back into member values

	Kick kickPlayer;
	Snare snarePlayer;
	Samples samples;
	HiHat hihatPlayer;
	Toms tomsPlayer;
	Claps clapsPlayer;

	NoteMapper noteMapper[Voice::count];

private:
	float FastTanh(const float x);
	uint8_t config[Voice::count * 2];							// Buffer to store config data (currently used to store midi note mapping]

	enum class ButtonMode {playNote, midiLearn, drumPattern};
	enum class MidiLearnState {off, lowNote, highNote};
	ButtonMode buttonMode;
	uint8_t midiLearnVoice;
	MidiLearnState midiLearnState;
	int16_t midiLearnCounter;
	uint32_t ledBackup[Voice::count];

};

extern VoiceManager voiceManager;
