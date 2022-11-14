#include "VoiceManager.h"
#include "Sequencer.h"

VoiceManager voiceManager;




VoiceManager::VoiceManager()
{
	for (uint8_t i = 0; i < Voice::count; ++i) {
		noteMapper[i].voice = i;
	}

	NoteMapper& k = noteMapper[Voice::kick];
	kickPlayer.noteMapper = &k;
	k.drumVoice = &kickPlayer;
	k.trigger = {GPIOB, 6, GPIOD, 1};
	k.pwmLed = {&TIM8->CCR4};			// PC9: Blue
	k.midiLow = 84;
	k.midiHigh = 84;

	NoteMapper& s = noteMapper[Voice::snare];
	snarePlayer.noteMapper = &s;
	s.drumVoice = &snarePlayer;
	s.trigger = {GPIOB, 5, GPIOD, 3};
	s.pwmLed = {&TIM8->CCR3};			// PC8: Red
	s.midiLow = 83;
	s.midiHigh = 83;

	NoteMapper& hh = noteMapper[Voice::hihat];
	hihatPlayer.noteMapper = &hh;
	hh.drumVoice = &hihatPlayer;
	hh.trigger = {GPIOE, 11, GPIOG, 10, GPIOD, 7};
	hh.pwmLed = {&TIM8->CCR2};			// PC7: White
	hh.midiLow = 78;
	hh.midiHigh = 82;

	NoteMapper& sa = noteMapper[samplerA];
	samples.sampler[0].noteMapper = &sa;
	sa.drumVoice = &samples;
	sa.voiceIndex = 0;
	sa.trigger = {GPIOB, 7, GPIOE, 14};
	sa.pwmLed = {&TIM4->CCR3};			// PD14: Green
	sa.midiLow = 60;
	sa.midiHigh = 67;

	NoteMapper& sb = noteMapper[samplerB];
	samples.sampler[1].noteMapper = &sb;
	sb.drumVoice = &samples;
	sb.voiceIndex = 1;
	sb.trigger = {GPIOG, 13, GPIOE, 15};
	sb.pwmLed = {&TIM4->CCR4};			// PD15: Yellow
	sb.midiLow = 72;
	sb.midiHigh = 77;

	NoteMapper& t = noteMapper[Voice::toms];
	tomsPlayer.noteMapper = &k;
	t.drumVoice = &tomsPlayer;
	t.midiLow = 48;
	t.midiHigh = 52;
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
	sequencer.Play();

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

	// Apply some soft clipping
	combinedOutput[left] = FastTanh(combinedOutput[left]);
	combinedOutput[right] = FastTanh(combinedOutput[right]);

	const float outputScale = 2147483648.0f * adjOutputScale;
	SPI2->TXDR = (int32_t)((combinedOutput[left] + adjOffset) *  outputScale);
	SPI2->TXDR = (int32_t)((combinedOutput[right] + adjOffset) * outputScale);

	for (auto& nm : noteMapper) {
		if (nm.drumVoice != nullptr && nm.voiceIndex == 0) {			// If voiceIndex is > 0 drum voice has multiple channels (eg sampler)
			nm.drumVoice->CalcOutput();
		}
	}
}


// Algorithm source: https://varietyofsound.wordpress.com/2011/02/14/efficient-tanh-computation-using-lamberts-continued-fraction/
float VoiceManager::FastTanh(const float x)
{
	const float x2 = x * x;
	const float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
	const float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
	return a / b;
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

			// Switch to next voice Once high value is assigned
			midiLearnState = MidiLearnState::lowNote;
			noteMapper[midiLearnVoice].pwmLed.Level(0.0f);
			if (++midiLearnVoice > Voice::samplerB) {
				midiLearnVoice = Voice::kick;
			}
		}
	} else {
		// Locate voice
		for (auto& note : noteMapper) {
			if (midiNote.noteValue >= note.midiLow && midiNote.noteValue <= note.midiHigh) {
				const uint32_t noteOffset = midiNote.noteValue - note.midiLow;
				const uint32_t noteRange = note.midiHigh - note.midiLow + 1;

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
	ButtonMode oldMode = buttonMode;
	if ((GPIOE->IDR & GPIO_IDR_ID1) == 0) {				// PE1: MIDI Learn
		if (oldMode != ButtonMode::midiLearn) {
			buttonMode = ButtonMode::midiLearn;
			sequencer.playing = false;					// Stop active sequence
			midiLearnState = MidiLearnState::lowNote;
			midiLearnVoice = 0;
			midiLearnCounter = 0;
		} else {
			// Pulse LED to show MIDI Learn state - slow is low note, fast is high note
			midiLearnCounter += midiLearnState == MidiLearnState::lowNote ? 5 : 10;
			noteMapper[midiLearnVoice].pwmLed.Level(midiLearnCounter > 0 ? 1.0f : 0.0f);
		}
	} else if ((GPIOC->IDR & GPIO_IDR_ID6) == 0) {		// PC6: Sequence Select
		buttonMode = ButtonMode::drumPattern;
	} else  {
		buttonMode = ButtonMode::playNote;
	}

	//	When switching to or from MIDI learn mode turn off all LEDs
	if (oldMode != buttonMode && (oldMode == ButtonMode::midiLearn || buttonMode == ButtonMode::midiLearn)) {
		for (auto& note : noteMapper) {
			note.pwmLed.setMinLevel(0);
		}
	}

	for (auto& note : noteMapper) {
		NoteMapper::TriggerType triggerType = note.trigger.IsOn();
		if (triggerType) {
			if (!note.trigger.buttonOn) {
				note.trigger.buttonOn = true;

				if (buttonMode == ButtonMode::midiLearn) {
					noteMapper[midiLearnVoice].pwmLed.Level(0.0f);
					midiLearnState = MidiLearnState::lowNote;
					midiLearnVoice = note.voice;

				} else if (buttonMode == ButtonMode::playNote || triggerType != NoteMapper::TriggerType::triggerBtn) {
					// Trigger input only used to play notes; Trigger 2 currently only used for open hihat
					note.drumVoice->Play(note.voiceIndex, triggerType == NoteMapper::TriggerType::trigger2 ? 128 : 0);

				} else if (buttonMode == ButtonMode::drumPattern) {
					sequencer.StartStop(note.voice);
				}
			}
		} else {
			note.trigger.buttonOn = false;
		}
	}

}


void VoiceManager::IdleTasks()
{
	for (auto& nm : noteMapper) {
		// If voiceIndex is > 0 drum voice has multiple channels (eg sampler)
		if (nm.drumVoice != nullptr && nm.voiceIndex == 0 && nm.drumVoice->playing) {
			nm.drumVoice->UpdateFilter();					// Check if filter coefficients need to be updated
		}
	}
}
