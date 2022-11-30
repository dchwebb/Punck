#include "Claps.h"
#include "VoiceManager.h"

void Claps::Play(const uint8_t voice, const uint32_t noteOffset, uint32_t noteRange, const float velocity)
{
	playing = true;

	velocityScale = velocity;
	level = config.initLevel;
	noteRange = noteRange == 0 ? 128 : noteRange;
//	sustainRate = 0.001f * sqrt((static_cast<float>(noteOffset) + 1.0f) / noteRange);		// note offset allows for longer sustained hits
}


void Claps::Play(const uint8_t voice, const uint32_t index)
{
	// Called when button is pressed
	Play(0, index, 0, 1.0f);
}


void Claps::CalcOutput()
{
	// MIDI notes will be queued from serial/usb interrupts to play in main I2S interrupt loop
	if (noteQueued) {
		Play(queuedNote.voice, queuedNote.noteOffset, queuedNote.noteRange, queuedNote.velocity);
		noteQueued = false;
	}

	if (playing) {
		const float rand1 = intToFloatMult * static_cast<int32_t>(RNG->DR);		// Initial random number used for noise

		level *= config.decay;

		outputLevel[left]  = velocityScale * (rand1 * level);
		outputLevel[right] = velocityScale * (rand1 * level);

		if (level < 0.00001f) {
			playing = false;
		}

	}
}


void Claps::UpdateFilter()
{
//	filter.Update();
}


uint32_t Claps::SerialiseConfig(uint8_t** buff, const uint8_t voiceIndex)
{
	*buff = reinterpret_cast<uint8_t*>(&config);
	return sizeof(config);
}


void Claps::StoreConfig(uint8_t* buff, const uint32_t len)
{
	if (len <= sizeof(config)) {
		memcpy(&config, buff, len);
	}
}


uint32_t Claps::ConfigSize()
{
	return sizeof(config);
}
