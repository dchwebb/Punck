#include <HiHat.h>
#include "VoiceManager.h"
#include <cstring>


void HiHat::Play(const uint8_t voice, const uint32_t noteOffset, uint32_t noteRange, const float velocity)
{
	playing = true;

	velocityScale = velocity * (static_cast<float>(ADC_array[ADC_HiHatLevel]) / 32768.0f);
	attack = true;
	attackLevel = 0.0f;
	noiseScale = intToFloatMult * config.noiseInitLevel;		// noise uses RND peripheral: scale 32 bit value to -1.0 to +1.0

	hpFilter.Init();
	hpFilterCutoff = config.hpInitCutoff;

	lpFilter.Init();
	lpFilterCutoff = config.lpInitCutoff;

	// Control over decay note index sets initial level; scaled by pot
	noteRange = noteRange == 0 ? 128 : noteRange;
	float closed = sqrt((static_cast<float>(noteOffset) + 1.0f) / noteRange);		// store 0.0f - 1.0f to for amount closed
	closed += (static_cast<float>(ADC_array[ADC_HiHatDecay]) / 65536.0f) - 0.5f;	// pot scales +/-0.5
	decayScale = std::min(0.9985f + (0.0015f * closed), 0.99998f);

	for (uint8_t i = 0; i < 6; ++i) {
		partialPos[i] = 0;
		partialLevel[i] = config.partialScale[i];
	}
}



void HiHat::Play(const uint8_t voice, const uint32_t index)
{
	// Called when button is pressed
	Play(0, index, 0, 1.0f);
}



void HiHat::CalcOutput()
{
	if (playing) {

		const int32_t noise = static_cast<int32_t>(RNG->DR);		// Get noise level here to give time for next noise value to be calculated

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
			hpFilterCutoff *= config.hpCutoffInc;

		}


		// Apply an envelope to the HP filter to allow more low frequencies through at the beginning of the note
		if (lpFilterCutoff > config.lpFinalCutoff) {
			lpFilterCutoff *= config.lpCutoffInc;

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
		if (velocityScale < 0.001f) {
			playing = false;
			currentLevel[left] = 0.0f;
			currentLevel[right] = 0.0f;
			hpFilterCutoff = config.hpInitCutoff;			// Reset filter cutoff so initial cofficients can be recalculated in idle time
			lpFilterCutoff = config.lpInitCutoff;
		}

		// Filter and scale the output levels
		outputLevel[0] = envScale * lpFilter.CalcFilter(hpFilter.CalcFilter(currentLevel[left], left), left) - 0.0001f;
		outputLevel[1] = envScale * lpFilter.CalcFilter(hpFilter.CalcFilter(currentLevel[right], right), right) - 0.0001f;

		noteMapper->pwmLed.Level(velocityScale);
	}

}


void HiHat::UpdateFilter()
{
	hpFilter.SetCutoff(hpFilterCutoff);
	lpFilter.SetCutoff(lpFilterCutoff);
}


uint32_t HiHat::SerialiseConfig(uint8_t** buff, const uint8_t voiceIndex)
{
	*buff = reinterpret_cast<uint8_t*>(&config);
	return sizeof(config);
}


void HiHat::StoreConfig(uint8_t* buff, const uint32_t len)
{
	if (buff != nullptr && len <= sizeof(config)) {
		memcpy(&config, buff, len);
	}

	// Apply any settings that are constant until configuration changes
	for (uint8_t i = 0; i < 6; ++i) {
		partialPeriod[i] = freqToSqPeriod / config.partialFreq[i];
	}
}

uint32_t HiHat::ConfigSize()
{
	return sizeof(config);
}


