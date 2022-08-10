#include <NoteHandler.h>
#include "samples.h"

NoteHandler noteHandler;

NoteHandler::NoteHandler()
{
	noteMapper[sampler1].gpioPin = 6;
	noteMapper[sampler1].gpioBank = GPIOC;
}

void NoteHandler::NoteOn(MidiHandler::MidiNote midiNote)
{
	if (buttonMode == ButtonMode::midiLearn) {
		noteMapper[(uint8_t)midiLearnVoice].midiLow = midiNote.noteValue;
	} else {
		samples.Play(1);
	}
}


void NoteHandler::CheckButtons()
{

	// Check mode select switch. Options: Play note; MIDI learn; drum pattern selector
	if (GPIOB->IDR & GPIO_IDR_ID3) {		// MIDI learn mode
		buttonMode = ButtonMode::midiLearn;
	} else {
		buttonMode = ButtonMode::playNote;
	}

	uint8_t i = 0;
	for (auto note : noteMapper) {
		if (note.gpioBank != nullptr) {
			if ((note.gpioBank->IDR & (1 << note.gpioPin)) == 0) {
				if (!note.buttonOn) {
					note.buttonOn = true;
					switch (buttonMode) {
					case ButtonMode::playNote:
						samples.Play(0);
						break;
					case ButtonMode::midiLearn:
						midiLearnVoice = (Voice)i;
						break;
					case ButtonMode::drumPattern:
						break;
					}
				}
			} else {
				note.buttonOn = false;
			}
		}
		++i;
	}
}
