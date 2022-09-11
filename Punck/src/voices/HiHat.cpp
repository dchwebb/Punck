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
	decayScale = 0.999 + (0.0009 * static_cast<float>(ADC_array[ADC_KickDecay]) / 65536.0f);

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



void HiHat::CalcOutput()
{
	if (playing) {

		int32_t noise = (int32_t)RNG->DR;		// Get noise level here to give time for next noise value to be calculated

		float currentLevel[2] = {};				// Store intermediate values of sample level for each channel

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

			currentLevel[left] += partialLevel[i];
		}


		// Apply an envelope to the HP filter to allow more low frequencies through at the beginning of the note
		if (hpFilterCutoff < config.hpFinalCutoff) {
			hpFilterCutoff *= 1.01f;
			hpFilter.SetCutoff(hpFilterCutoff);
		}


		// Add a burst of noise at the beginning of the note (both channels use the sample partials, but different noise)
		noiseScale *= config.noiseDecay;
		currentLevel[left] += noiseScale * noise;
		currentLevel[right] = currentLevel[left] + noiseScale * (int32_t)RNG->DR;


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


		// End when note volume is 0
		if (velocityScale < 0.0001f) {
			playing = false;
			currentLevel[left] = 0.0f;
			currentLevel[right] = 0.0f;
			noteMapper->led.Off();
		}

		// Filter and scale the output levels
		outputLevel[0] = envScale * hpFilter.CalcFilter(currentLevel[left], left) - 0.0001f;
		outputLevel[1] = envScale * hpFilter.CalcFilter(currentLevel[right], right) - 0.0001f;;
	}

}


void HiHat::UpdateFilter()
{

}


uint32_t HiHat::SerialiseConfig(uint8_t** buff)
{
	*buff = (uint8_t*)&config;
	//memcpy(buff, &config, sizeof(config));
	return sizeof(config);
}


void HiHat::StoreConfig(uint8_t* buff, uint32_t len)
{
	if (len <= sizeof(config)) {
		memcpy(&config, buff, len);
	}
}



