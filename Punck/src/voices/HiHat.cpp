#include <HiHat.h>
#include "VoiceManager.h"
#include <cstring>


void HiHat::Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, float velocity)
{

	// Called when accessed from MIDI
	playing = true;
	noteMapper->led.On();

	velocityScale = velocity;
	attack = true;
	attackLevel = 0.0f;
	noiseScale = intToFloatMult * config.noiseInitLevel;		// noise uses RND peripheral: scale 32 bit value to -1.0 to +1.0

	hpFilter.Init();
	hpFilterCutoff = config.hpInitCutoff;
	hpFilter.SetCutoff(hpFilterCutoff);

	// Control over decay - FIXME change ADC for production
	decayScale = 0.999 + (0.001 * static_cast<float>(ADC_array[ADC_KickDecay]) / 65536.0f);

	for (uint8_t i = 0; i < 6; ++i) {
		partialPeriod[i] = freqToSqPeriod / config.partialFreq[i];
		partialPos[i] = 0;
		partialLevel[i] = config.partialScale[i];
	}


}


void HiHat::Play(uint8_t voice, uint32_t index)
{
	// Called when button is pressed
	Play(0, 0, 0, 1.0f);
}



bool fmOn = true;

void HiHat::CalcOutput()
{
	if (playing) {
		currentLevel = 0.0f;

		for (uint8_t i = 0; i < 6; ++i) {
			if (config.partialFM[i] > 0 && partialLevel[0] > 0.0f) {			// Add some frequency modulation to some partials
				partialPos[i] += config.partialFM[i];
			} else {
				++partialPos[i];
			}

			if (partialPos[i] > partialPeriod[i]) {
				partialPos[i] = 0;
				partialLevel[i] = -partialLevel[i];
			}

			currentLevel += partialLevel[i];
		}


		// Add a burst of noise at the beginning of the note
		noiseScale *= config.noiseDecay;
		currentLevel += noiseScale * (int32_t)RNG->DR;

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
			velocityScale *= decayScale;
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



