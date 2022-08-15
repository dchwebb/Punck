#include <NoteHandler.h>
#include "samples.h"

NoteHandler noteHandler;


NoteHandler::NoteHandler()
{
	for (uint8_t i = 0; i < 9; ++i) {
		noteMapper[i].voice = (Voice)i;
	}

	noteMapper[sampler1].gpioPinBtn = 6;
	noteMapper[sampler1].gpioBankBtn = GPIOC;
	noteMapper[sampler1].led = {GPIOB, 0};				// PB0: Green
	noteMapper[sampler1].midiLow = 72;
	noteMapper[sampler1].midiHigh = 79;

	noteMapper[sampler2].led = {GPIOE, 1};				// PE1: Yellow LED nucleo
	noteMapper[sampler2].midiLow = 60;
	noteMapper[sampler2].midiHigh = 67;

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
		for (auto note : noteMapper) {
			if (midiNote.noteValue >= note.midiLow && midiNote.noteValue <= note.midiHigh) {
				uint32_t noteOffset = midiNote.noteValue - note.midiLow;
				uint32_t noteRange = note.midiHigh - note.midiLow + 1;
				if (note.voice == sampler1) {
					samples.Play(samples.SamplePlayer::playerA, noteOffset, noteRange);
				} else if (note.voice == sampler2) {
					samples.Play(samples.SamplePlayer::playerB, noteOffset, noteRange);
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
			for (auto note : noteMapper) {
				if (note.led.gpioBank) {
					note.led.Off();
				}
			}
		}
		buttonMode = ButtonMode::playNote;
	}

	for (auto note : noteMapper) {
		if (note.gpioBankBtn != nullptr) {
			if ((note.gpioBankBtn->IDR & (1 << note.gpioPinBtn)) == 0) {
				if (!note.buttonOn) {
					note.buttonOn = true;
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
				note.buttonOn = false;
			}
		}
	}
}
