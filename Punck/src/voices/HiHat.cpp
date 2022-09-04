#include <HiHat.h>
#include "VoiceManager.h"
#include <cstring>

uint32_t dbgPos = 0;
uint32_t dbgCnt = 48;

float hpCutoff = 0.27f;
float bpCutoff = 3000.0f;

void HiHat::Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, float velocity)
{
//	if (playing) {
//		playing = false;
//		noteMapper->led.Off();
//	} else {

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
		hpFilter.Init();
		hpFilter.SetCutoff(hpCutoff);
		bpFilter.SetCutoff(bpCutoff, config.bpFilterQ);
		bpFilter.Init();
		bpEnvLevel = 1.0f;
		hpEnvLevel = 1.0f;
//	}
}


void HiHat::Play(uint8_t voice, uint32_t index)
{
	// Called when button is pressed
	Play(0, 0, 0, 1.0f);
}


float hhPartialInc[6] = {
						 -569.0f / ((float)systemSampleRate / 4.0f),
						 621.0f / ((float)systemSampleRate / 4.0f),
						 -1559.0f / ((float)systemSampleRate / 4.0f),
						 2056.0f / ((float)systemSampleRate / 4.0f),
						 -3300.0f / ((float)systemSampleRate / 4.0f),
						 5515.0f / ((float)systemSampleRate / 4.0f),
						};
float hhPartialLevel[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
float hhPartialScale[6] = {0.2f, 0.3f, 0.4f, 0.8f, 0.75f, 0.85};

void HiHat::CalcOutput()
{
	if (playing) {
		++position;
		float output = 0.0f;

		// FM
		hhPartialInc[4] += hhPartialLevel[1];
		hhPartialInc[5] += hhPartialLevel[0];
		hhPartialInc[3] += hhPartialLevel[2];

		for (uint8_t i = 0; i < 6; ++i) {
			hhPartialLevel[i] += hhPartialInc[i];
			if (hhPartialLevel[i] > 1.0f) {
				hhPartialLevel[i] = 1.0f;
				hhPartialInc[i] = -hhPartialInc[i];
			}
			if (hhPartialLevel[i] < -1.0f) {
				hhPartialLevel[i] = -1.0f;
				hhPartialInc[i] = -hhPartialInc[i];
			}
			output += (hhPartialLevel[i] > 0 ? 1.0f : -1.0f) * hhPartialScale[i];
			//output += hhPartialLevel[i] * hhPartialScale[i];
		}





		//velocityScale = 1.0f;

		//float noise = intToFloatMult * (int32_t)RNG->DR;


		velocityScale *= config.decay;
		currentLevel = velocityScale * output - 0.00001f;
		//DAC1->DHR12R1 = velocityScale * 4095.0f;		// Output envelope to DAC for debugging

		if (velocityScale < 0.00001f) {
			playing = false;
			currentLevel = 0.0f;
			//velocityScale = 0.0f;
			noteMapper->led.Off();
		}

		//outputLevel[0] = hpFilter.CalcFilter(currentLevel, left);
		outputLevel[0] = hpFilter.CalcFilter(currentLevel, left);
		outputLevel[1] = outputLevel[0];
	}

	/*
	if (playing) {
		++position;
		float output = 0.0f;

		// Generate FM signal
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

		//currentLevel = intToFloatMult * (int32_t)RNG->DR;		// Left channel random number used for noise
		//float noise = intToFloatMult * (int32_t)RNG->DR;

		// Create fast decay of BP filter to 15kHz over ~370ms (9600 samples)
		if (bpEnvLevel >= 0.0001) {
			bpEnvLevel *= config.bpEnvMult;

			// Set fast decay band pass filter
			float bpCutoff = config.bpFilterFreq + (systemMaxFreq - config.bpFilterFreq) * bpEnvLevel;
			if (bpCutoff < 15000.0f) {
				bpCutoff = 15000.0f;
			}
			//float cutoff = ((float)ADC_array[ADC_Filter_Pot] / 65536.0f) * 8000.0f;
			bpFilter.SetCutoff(bpCutoff, config.bpFilterQ);
			output = config.bpEnvScale * bpEnvLevel * bpFilter.CalcFilter(carrierLevel);
		}

		// Create slow envelope controlling high pass filter
		hpEnvLevel *= config.hpEnvMult;
		float hpCutoff = (config.hpFilterFreq + (systemMaxFreq - config.hpFilterFreq) * hpEnvLevel) / systemMaxFreq;
		//(1570.0f + (18430.0f * hpEnvLevel) / 20000.0f);
		hpFilter.SetCutoff(hpCutoff);
		output += config.hpEnvScale * hpFilter.CalcFilter(carrierLevel, left);

		DAC1->DHR12R1 = hpCutoff * 4095.0f;		// Output envevlope to DAC for debugging

		velocityScale *= config.decay;
		currentLevel = velocityScale * output;
		//DAC1->DHR12R1 = velocityScale * 4095.0f;		// Output envelope to DAC for debugging

		if (velocityScale < 0.00001f) {
			playing = false;
			//currentLevel = 0.0f;
			//velocityScale = 0.0f;
			noteMapper->led.Off();
		}

		outputLevel[0] = currentLevel;
		//outputLevel[0] = velocityScale * bpFilter.CalcFilter(currentLevel);
		//outputLevel[0] = velocityScale * hpFilter.CalcFilter(currentLevel, left);
		outputLevel[1] = outputLevel[0];
	}
	*/
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



