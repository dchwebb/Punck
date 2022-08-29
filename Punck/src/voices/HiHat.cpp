#include <HiHat.h>
#include "VoiceManager.h"
#include <cstring>

void HiHat::Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, float velocity)
{
	// Called when accessed from MIDI
	playing = true;
	position = 0.0f;
	carrierPos = 0.0f;
	modulatorPos = 0.0f;
	noteMapper->led.On();
	velocityScale = velocity;
	currentLevel = 1.0f;
	carrierLevel = 1.0f;
	modulatorHigh = true;


}


void HiHat::Play(uint8_t voice, uint32_t index)
{
	// Called when button is pressed
	Play(0, 0, 0, 1.0f);
}


void HiHat::CalcOutput()
{
	if (playing) {
		float carrierPeriod = ((float)systemSampleRate / 2.0f) / config.carrierFreq;
		float modHighPeriod = config.modulatorDuty * ((float)systemSampleRate) / config.modulatorFreq;
		float modLowPeriod = (1 - config.modulatorDuty) * ((float)systemSampleRate) / config.modulatorFreq;
		velocityScale *= config.decay;

		carrierPos += (modulatorHigh ? config.modulatorHighMult : config.modulatorLowMult);
		if (carrierPos >= carrierPeriod) {
			carrierPos = 0.0f;
			carrierLevel *= -1.0f;
		}

		++modulatorPos;
		if (modulatorPos >= (modulatorHigh ? modHighPeriod : modLowPeriod)) {
			modulatorPos = 0.0f;
			modulatorHigh = !modulatorHigh;
		}
		currentLevel = carrierLevel;
		//currentLevel = intToFloatMult * (int32_t)RNG->DR;		// Left channel random number used for noise

		if (position++ > 96000.0f) {
			playing = false;
			currentLevel = 0.0f;
			velocityScale = 0.0f;
			noteMapper->led.Off();
		}

		outputLevel[0] = velocityScale * filter.CalcFilter(currentLevel, left);
		outputLevel[1] = outputLevel[0];
	}
}


void HiHat::UpdateFilter()
{
	filter.Update();
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



