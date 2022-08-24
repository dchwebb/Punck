#include <Snare.h>
#include "NoteHandler.h"

void Snare::Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange)
{
	// Called when accessed from MIDI (different note offsets for different tuning?)
	partial1Inc = FrequencyToIncrement(partial1Freq);		// First Mode 0,1 frequency
	partial2Inc = FrequencyToIncrement(partial1Freq * 1.833f);		// Second Mode 0,1 frequency

	partial1pos = 0.3f;
	partial2pos = 0.0f;
	currentLevel = 0.0f;
	partial1Level = partial1InitLevel;
	partial2Level = partial2InitLevel;
	randLevel = randInitLevel;
	playing = true;
	noteMapper->led.On();
}


void Snare::Play(uint8_t voice, uint32_t index)
{
	// Called when button is pressed
	Play(0, 0, 0);
}


void Snare::CalcOutput()
{
	if (playing) {
		float rand1 = intToFloatMult * (int32_t)RNG->DR;

		partial1pos += partial1Inc;					// Set current poition in sine wave
		partial2pos += partial2Inc;					// Set current poition in sine wave
		float decaySpeed = 0.9991 + 0.00055f * static_cast<float>(ADC_array[ADC_KickDecay]) / 65536.0f;
		partial1Level = partial1Level * decaySpeed;
		partial2Level = partial2Level * decaySpeed;
		randLevel *= randDecay;

		// Fixme should decrement at different rates
		float sharedOutput = (std::sin(partial1pos) * partial1Level) + (std::sin(partial2pos) * partial2Level);

		float rand2 = intToFloatMult * (int32_t)RNG->DR;		// Get right channel random number here to give time to update

		if (rand1 == rand2) {
			volatile int susp = 1;
		}

		outputLevel[left] = sharedOutput +	(rand1 * randLevel);
		outputLevel[right] = sharedOutput +	(rand2 * randLevel);

		//currentLevel = std::sin(partial1pos);

		if (partial1Level <= 0.00001f && std::abs(partial1Level) < 0.00001f) {
			playing = false;
			noteMapper->led.Off();
		}
	}
	outputLevel[left] = filter.CalcFilter(outputLevel[0], left);
	outputLevel[right] = filter.CalcFilter(outputLevel[1], right);

}

void Snare::UpdateFilter()
{
	filter.Update();
}
