#include <Snare.h>
#include <VoiceManager.h>

void Snare::Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, float velocity)
{
	// Called when accessed from MIDI (different note offsets for different tuning?)
	partialInc[0] = FreqToInc(config.baseFreq);		// First Mode 0,1 frequency
	partialpos[0] = 0.3f;						// Create discontinuity to create initial click
	partialpos[1] = 0.0f;

	for (uint8_t i = 0; i < partialCount; ++i) {
		partialLevel[i] = config.partialInitLevel[i];
		partialInc[i] = FreqToInc(config.baseFreq * config.partialFreqOffset[i]);
	}
	noiseLevel = config.noiseInitLevel;
	velocityScale = velocity;
	playing = true;
	noteMapper->led.On();
}


void Snare::Play(uint8_t voice, uint32_t index)
{
	// Called when button is pressed
	Play(0, 0, 0, 1.0f);
}


void Snare::CalcOutput()
{
	if (playing) {
		float rand1 = intToFloatMult * (int32_t)RNG->DR;		// Left channel random number used for noise
		float adcDecay = 0.00055f * static_cast<float>(ADC_array[ADC_KickDecay]) / 65536.0f;		// FIXME - use dedicated ADC

		float partialOutput = 0.0f;
		bool partialsInaudible = true;							// To calculate when to terminate note
		for (uint8_t i = 0; i < partialCount; ++i) {
			partialInc[i] *= config.partialPitchDrop;
			partialpos[i] += partialInc[i];						// Set current poition in sine wave
			partialLevel[i] *= config.partialDecay + adcDecay;
			partialOutput += std::sin(partialpos[i]) * partialLevel[i];

			if (partialLevel[i] > 0.00001f) {
				partialsInaudible = false;
			}
		}
		noiseLevel *= config.noiseDecay + adcDecay;

		float rand2 = intToFloatMult * (int32_t)RNG->DR;		// Get right channel noise value: calc here to give time for peripheral to update

		if (rand1 == rand2) {
			volatile int susp = 1;
		}

		outputLevel[left]  = velocityScale * filter.CalcFilter(partialOutput + (rand1 * noiseLevel), left);
		outputLevel[right] = velocityScale * filter.CalcFilter(partialOutput + (rand2 * noiseLevel), right);

		if (partialsInaudible) {
			playing = false;
			noteMapper->led.Off();
		}
	}

}


void Snare::UpdateFilter()
{
	filter.Update();
}


uint32_t Snare::SerialiseConfig(uint8_t* buff)
{
	memcpy(buff, &config, sizeof(config));
	return sizeof(config);
}


void Snare::ReadConfig(uint8_t* buff, uint32_t len)
{
	if (len <= sizeof(config)) {
		memcpy(&config, buff, len);
	}
}
