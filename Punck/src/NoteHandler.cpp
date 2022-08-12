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
	noteMapper[sampler1].gpioPinLED = 0;			// PB0: Green LED nucleo
	noteMapper[sampler1].gpioBankLED = GPIOB;
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
		}
	} else {
		// Locate voice
		for (auto note : noteMapper) {
			if (midiNote.noteValue >= note.midiLow && midiNote.noteValue <= note.midiHigh) {
				uint32_t noteOffset = midiNote.noteValue - note.midiLow;
				uint32_t noteRange = note.midiHigh - note.midiLow + 1;
				samples.Play(1, noteOffset, noteRange);
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
		buttonMode = ButtonMode::playNote;
	}

	for (auto note : noteMapper) {
		if (note.gpioBankBtn != nullptr) {
			if ((note.gpioBankBtn->IDR & (1 << note.gpioPinBtn)) == 0) {
				if (!note.buttonOn) {
					note.buttonOn = true;
					switch (buttonMode) {
					case ButtonMode::playNote:
						samples.Play(0);
						break;
					case ButtonMode::midiLearn:
						note.gpioBankLED->ODR |= (1 << note.gpioPinLED);
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
