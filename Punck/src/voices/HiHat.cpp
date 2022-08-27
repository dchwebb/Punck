#include <HiHat.h>
#include "VoiceManager.h"

void HiHat::Play(uint8_t voice, uint32_t noteOffset, uint32_t noteRange, float velocity)
{
	// Called when accessed from MIDI
	position = 0.0f;
	noteMapper->led.On();
	velocityScale = velocity;
}


void HiHat::Play(uint8_t voice, uint32_t index)
{
	// Called when button is pressed
	Play(0, 0, 0, 1.0f);
}


void HiHat::CalcOutput()
{
	currentLevel = 0.0f;
	position += 1.0f;
	if (position > 96000.0f) {
		noteMapper->led.Off();
	}

	outputLevel[0] = velocityScale * filter.CalcFilter(currentLevel, left);
	outputLevel[1] = outputLevel[0];
}


void HiHat::UpdateFilter()
{
	//filter.Update();
}


uint32_t HiHat::SerialiseConfig(uint8_t* buff)
{
	return 0;
}

void HiHat::ReadConfig(uint8_t* buff, uint32_t len)
{
}
