#include <HiHat.h>
#include "VoiceManager.h"
#include <cstring>

uint32_t dbgPos = 0;
uint32_t dbgCnt = 48;

void HiHat::Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, float velocity)
{
	if (playing) {
		playing = false;
	} else {
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
		hpFilter.SetCutoff(0);
		dbgPos = 0;
		dbgCnt = 48;
	}
}


void HiHat::Play(uint8_t voice, uint32_t index)
{
	// Called when button is pressed
	Play(0, 0, 0, 1.0f);
}


static constexpr uint32_t dbgSize = 3000;
float __attribute__((section (".ram_d1_data"))) dbgCutoff[dbgSize];
float decayRate = 0.1;

void HiHat::CalcOutput()
{
	if (playing) {
		++position;

		// Calculate high pass filter
		/*
		float posInTime = (1.0f / systemSampleRate) * (position + 40);
		float cutoff = 0.1f * (2 * decayRate * sqrt(posInTime) - posInTime) / (decayRate * decayRate);
		*/
		/*
		float posInTime = (position / systemSampleRate) + 0.4;
		float cutoff = sin(1.0f / posInTime);

		hpFilter.SetCutoff(cutoff);
	*/
		// Create fast decay of BP filter to 1kHz over 200ms (9600 samples)
		float cutoff = 1000.0f + (20000.0f * (1.0f - (float)position / 9600.0f));
		if (cutoff < 1000.0f) {
			cutoff = 1000.0f;
		}

		DAC1->DHR12R1 = (cutoff / 20000.0f) * 4095.0f;		// DAC1->DHR12R2

		//float cutoff = ((float)ADC_array[ADC_Filter_Pot] / 65536.0f) * 8000.0f;
		bpFilter.SetCutoff(cutoff, config.Q);

		if (dbgPos < dbgSize) {
			if (dbgCnt++ == 48) {
				dbgCnt = 0;
			}
			dbgCutoff[dbgPos++] = cutoff;
		}

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

//		if (position > 96000.0f) {
//			playing = false;
//			currentLevel = 0.0f;
//			velocityScale = 0.0f;
//			noteMapper->led.Off();
//		}

		outputLevel[0] = currentLevel;
		//outputLevel[0] = velocityScale * bpFilter.CalcFilter(currentLevel);
		//outputLevel[0] = velocityScale * hpFilter.CalcFilter(currentLevel, left);
		outputLevel[1] = outputLevel[0];
	}
}


void HiHat::UpdateFilter()
{
	//filter.Update();
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



