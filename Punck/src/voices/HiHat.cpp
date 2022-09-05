#include <HiHat.h>
#include "VoiceManager.h"
#include <cstring>


void HiHat::Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, float velocity)
{
//	if (playing) {
//		playing = false;
//		noteMapper->led.Off();
//	} else {

		// Called when accessed from MIDI
		playing = true;
		noteMapper->led.On();
		velocityScale = velocity;
		currentLevel = 1.0f;
		hpFilter.Init();
		hpFilterCutoff = config.hpInitCutoff;
		hpFilter.SetCutoff(hpFilterCutoff);
		attack = true;
		attackLevel = 0.0f;

		// Scale high frequency components according to brightness setting
		float brightness = 50.0f * (1.0f - ((float)ADC_array[ADC_Filter_Pot] / 65536.0f));

		for (uint8_t i = 0; i < 2; ++i) {
			//partialInc[i] = (config.partialFreq[i] - brightness) * freqToTriPeriod;
			//partialInc[i] = (config.partialFreq[i]) * freqToTriPeriod;
		}
//	}
}


void HiHat::Play(uint8_t voice, uint32_t index)
{
	// Called when button is pressed
	Play(0, 0, 0, 1.0f);
}





void HiHat::CalcOutput()
{
	if (playing) {
		currentLevel = 0.0f;

		// Apply FM to the higher three partials from the lower three
		partialInc[3] += partialLevel[1];
		partialInc[4] += partialLevel[2];
		partialInc[5] += partialLevel[0];

		for (uint8_t i = 0; i < 6; ++i) {
			partialLevel[i] += partialInc[i];
			if (partialLevel[i] > 1.0f) {
				partialLevel[i] = 1.0f;
				partialInc[i] = -partialInc[i];
			}
			if (partialLevel[i] < -1.0f) {
				partialLevel[i] = -1.0f;
				partialInc[i] = -partialInc[i];
			}

			// Convert to square wave
			currentLevel += (partialLevel[i] > 0 ? 1.0f : -1.0f) * config.partialScale[i];
		}

		// Add a burst of noise at the beginning of the note
		currentLevel += velocityScale * noiseScale * (int32_t)RNG->DR;

		// Apply an envelope to the HP filter to allow more low frequencies through at the beginning of the note
		if (hpFilterCutoff < config.hpFinalCutoff) {
			hpFilterCutoff *= 1.01f;
			hpFilter.SetCutoff(hpFilterCutoff);
		}

		// Apply a short attack fade in then longer decay
		float envScale;
		if (attack) {
			attackLevel += config.attackInc;
			if (attackLevel >= velocityScale) {
				attackLevel = velocityScale;
				attack = false;
			}
			envScale = attackLevel;

		} else {
			velocityScale *= config.decay;
			envScale = velocityScale;
		}
		DAC1->DHR12R1 = envScale * 4095.0f;		// Output envelope to DAC for debugging


		// End when note volume is 0
		if (velocityScale < 0.0001f) {
			playing = false;
			currentLevel = 0.0f;
			noteMapper->led.Off();
			DAC1->DHR12R1 = 0;		// Output envelope to DAC for debugging
		}

		outputLevel[0] = envScale * hpFilter.CalcFilter(currentLevel, left) - 0.0001f;
		outputLevel[1] = outputLevel[0];
	}

}


void HiHat::UpdateFilter()
{
	//lpFilter.Update();

}


uint32_t HiHat::SerialiseConfig(uint8_t* buff)
{
	memcpy(buff, &config, sizeof(config));
	return sizeof(config);
}

void HiHat::ReadConfig(uint8_t* buff, uint32_t len)
{
	if (len <= sizeof(config)) {
		memcpy(&config, buff, len);
	}
}



