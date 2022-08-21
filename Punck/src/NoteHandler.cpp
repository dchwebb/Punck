#include <NoteHandler.h>
#include "samples.h"
#include "Kick.h"

NoteHandler noteHandler;


NoteHandler::NoteHandler()
{
	for (uint8_t i = 0; i < 9; ++i) {
		noteMapper[i].voice = (Voice)i;
	}

	noteMapper[samplerA].btn = {GPIOC, 6};
	noteMapper[samplerA].led = {GPIOB, 0};				// PB0: Green
	noteMapper[samplerA].midiLow = 72;
	noteMapper[samplerA].midiHigh = 79;

	noteMapper[samplerB].led = {GPIOE, 1};				// PE1: Yellow LED nucleo
	noteMapper[samplerB].midiLow = 60;
	noteMapper[samplerB].midiHigh = 67;

	noteMapper[Voice::kick].led = {GPIOB, 14};			// PB14: Red LED nucleo
	noteMapper[Voice::kick].midiLow = 84;
	noteMapper[Voice::kick].midiHigh = 84;
}


uint32_t leftOverflow = 0, rightOverflow = 0;	// Debug
void NoteHandler::Output()
{
	CheckButtons();										// Handle buttons playing note or activating MIDI learn

	float leftOutput =  samples.mixedSamples[0] +
						kickPlayer.ouputLevel * 1.0f;
	float rightOutput = samples.mixedSamples[1] +
						kickPlayer.ouputLevel * 1.0f;

	if (std::abs(leftOutput) > 1.0f)  { ++leftOverflow; }		// Debug
	if (std::abs(rightOutput) > 1.0f) { ++rightOverflow; }

	SPI2->TXDR = (int32_t)(leftOutput * 2147483648.0f);
	SPI2->TXDR = (int32_t)(rightOutput * 2147483648.0f);

	kickPlayer.CalcOutput();
	samples.CalcOutput();
}


void NoteHandler::VoiceLED(Voice v, bool on)
{
	if (on) {
		noteMapper[v].led.On();
	} else {
		noteMapper[v].led.Off();
	}
}


void NoteHandler::NoteOn(MidiHandler::MidiNote midiNote)
{
	if (buttonMode == ButtonMode::midiLearn) {
		NoteMapper& n = noteMapper[(uint8_t)midiLearnVoice];
		if (midiLearnState == MidiLearnState::lowNote) {
			n.midiLow = midiNote.noteValue;
			n.midiHigh = midiNote.noteValue;
			midiLearnState = MidiLearnState::highNote;
		} else {
			n.midiHigh = midiNote.noteValue;
			if (n.midiLow > n.midiHigh) {
				std::swap(n.midiLow, n.midiHigh);
			}
			n.led.Off();		// FIXME - logic to jump to next voice for midi learn
		}
	} else {
		// Locate voice
		for (auto& note : noteMapper) {
			if (midiNote.noteValue >= note.midiLow && midiNote.noteValue <= note.midiHigh) {
				uint32_t noteOffset = midiNote.noteValue - note.midiLow;
				uint32_t noteRange = note.midiHigh - note.midiLow + 1;

				switch (note.voice) {
				case Voice::samplerA:
					samples.Play(samples.SamplePlayer::playerA, noteOffset, noteRange);
					break;
				case Voice::samplerB:
					samples.Play(samples.SamplePlayer::playerB, noteOffset, noteRange);
					break;
				case Voice::kick:
					kickPlayer.Play(noteOffset, noteRange);
					break;
				default:
					break;
				}

			}
		}
	}
}


void NoteHandler::CheckButtons()
{

	// Check mode select switch. Options: Play note; MIDI learn; drum pattern selector
	if (GPIOB->IDR & GPIO_IDR_ID3) {			// MIDI learn mode
		buttonMode = ButtonMode::midiLearn;
	} else {
		if (buttonMode == ButtonMode::midiLearn) {
			// Switch off all LEDs
			for (auto& note : noteMapper) {
				if (note.led.gpioBank) {
					note.led.Off();
				}
			}
		}
		buttonMode = ButtonMode::playNote;
	}

	for (auto& note : noteMapper) {
		if (note.btn.IsOn()) {
			if (!note.btn.buttonOn) {
				note.btn.buttonOn = true;

				switch (buttonMode) {
				case ButtonMode::playNote:
					samples.Play(samples.SamplePlayer::playerA, 0);
					break;
				case ButtonMode::midiLearn:
					note.led.On();
					midiLearnState = MidiLearnState::lowNote;
					midiLearnVoice = note.voice;
					break;
				case ButtonMode::drumPattern:
					break;
				}
			}
		} else {
			note.btn.buttonOn = false;
		}
	}
}
