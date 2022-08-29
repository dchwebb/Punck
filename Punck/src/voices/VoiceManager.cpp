#include "VoiceManager.h"

VoiceManager voiceManager;


VoiceManager::VoiceManager()
{
	for (uint8_t i = 0; i < 9; ++i) {
		noteMapper[i].voice = i;
	}

	NoteMapper& sa = noteMapper[samplerA];
	samples.sampler[0].noteMapper = &sa;
	sa.drumVoice = &samples;
	sa.voiceIndex = 0;
//	sa.btn = {GPIOC, 6};
	sa.led = {GPIOB, 0};				// PB0: Green
	sa.midiLow = 72;
	sa.midiHigh = 79;

	NoteMapper& sb = noteMapper[samplerB];
	samples.sampler[1].noteMapper = &sb;
	sb.drumVoice = &samples;
	sb.voiceIndex = 1;
	sb.led = {GPIOE, 1};				// PE1: Yellow LED nucleo
	sb.midiLow = 60;
	sb.midiHigh = 67;

	NoteMapper& k = noteMapper[Voice::kick];
	kickPlayer.noteMapper = &k;
	k.drumVoice = &kickPlayer;
	//k.btn = {GPIOC, 6};
	k.led = {GPIOB, 14};				// PB14: Red LED nucleo
	k.midiLow = 84;
	k.midiHigh = 84;

	NoteMapper& s = noteMapper[Voice::snare];
	snarePlayer.noteMapper = &s;
	s.drumVoice = &snarePlayer;
	//s.btn = {GPIOC, 6};
	s.led = {GPIOB, 14};				// PB14: Red LED nucleo
	s.midiLow = 83;
	s.midiHigh = 83;

	NoteMapper& hh = noteMapper[Voice::hatClosed];
	hihatPlayer.noteMapper = &hh;
	hh.drumVoice = &hihatPlayer;
	hh.btn = {GPIOC, 6};
	hh.led = {GPIOE, 1};				// PE1: Yellow LED nucleo
	hh.midiLow = 82;
	hh.midiHigh = 82;

}


uint32_t leftOverflow = 0, rightOverflow = 0;	// Debug
//float leftOutput = 0.0f, rightOutput = 0.0f;
//float outInc = 0.005;
//uint32_t waitCrossing = 0;
float adjOffset = -0.03f;
float adjOutputScale = 0.92f;
void VoiceManager::Output()
{
	CheckButtons();										// Handle buttons playing note or activating MIDI learn

/*
	// Test code for calculating offsets and maximum levels before ADC distorts
	leftOutput += outInc;
	if (std::abs(leftOutput) > 1.0f) {
		leftOutput = (leftOutput > 0.0f) ? 1.0f : -1.0f;
		outInc *= -1.0f;
		waitCrossing = 0;
	}
	if (leftOutput <= 0.0f && outInc < 0.0f && waitCrossing < 10000) {
		leftOutput = 0.0f;
		waitCrossing++;
	}
	rightOutput = leftOutput;
*/

	float combinedOutput[2] = {0.0f, 0.0f};
	for (auto& nm : noteMapper) {
		if (nm.drumVoice != nullptr && nm.voiceIndex == 0) {			// If voiceIndex is > 0 drum voice has multiple channels (eg sampler)
			combinedOutput[left]  += nm.drumVoice->outputLevel[left];
			combinedOutput[right] += nm.drumVoice->outputLevel[right];
		}
	}


	if (std::abs(combinedOutput[left])  > 1.0f) { ++leftOverflow; }		// Debug
	if (std::abs(combinedOutput[right]) > 1.0f) { ++rightOverflow; }

	float outputScale = 2147483648.0f * adjOutputScale;
	SPI2->TXDR = (int32_t)((combinedOutput[left] + adjOffset) *  outputScale);
	SPI2->TXDR = (int32_t)((combinedOutput[right] + adjOffset) * outputScale);

	for (auto& nm : noteMapper) {
		if (nm.drumVoice != nullptr && nm.voiceIndex == 0) {			// If voiceIndex is > 0 drum voice has multiple channels (eg sampler)
			nm.drumVoice->CalcOutput();
		}
	}
}


void VoiceManager::NoteOn(MidiHandler::MidiNote midiNote)
{
	if (buttonMode == ButtonMode::midiLearn) {
		NoteMapper& n = noteMapper[midiLearnVoice];
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

				if (note.drumVoice) {
					note.drumVoice->Play(note.voiceIndex, noteOffset, noteRange, static_cast<float>(midiNote.velocity) / 127.0f);
				}
			}
		}
	}
}


void VoiceManager::CheckButtons()
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
					if (note.drumVoice) {
						note.drumVoice->Play(note.voiceIndex, 0);
					}
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


void VoiceManager::IdleTasks()
{
	for (auto& nm : noteMapper) {
		if (nm.drumVoice != nullptr && nm.voiceIndex == 0 && nm.drumVoice->playing) {		// If voiceIndex is > 0 drum voice has multiple channels (eg sampler)
			nm.drumVoice->UpdateFilter();					// Check if filter coefficients need to be updated
		}
	}
}