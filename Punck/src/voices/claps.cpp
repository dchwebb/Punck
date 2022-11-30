#include "Claps.h"
#include "VoiceManager.h"

void Claps::Play(const uint8_t voice, const uint32_t noteOffset, uint32_t noteRange, const float velocity)
{
	playing = true;

	//velocityScale = velocity;
	velocityScale = 1.0f;
	level = config.initLevel;
	state = State::hit1;
	stateCounter = 0;

//	noteRange = noteRange == 0 ? 128 : noteRange;
//	sustainRate = 0.001f * sqrt((static_cast<float>(noteOffset) + 1.0f) / noteRange);		// note offset allows for longer sustained hits

	filter.SetCutoff(config.filterCutoff, config.filterQ);
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
		float output = intToFloatMult * static_cast<int32_t>(RNG->DR);		// Initial random number used for noise
		output = filter.CalcFilter(output);

		++stateCounter;

		switch (state) {
		case State::start:
			output = 1.0f;
			if (stateCounter > 5) {
				state = State::hit1;
			}
			break;
		case State::hit1:
			if (stateCounter > 460) {		// approx 9.6ms
				level = config.initLevel;
				stateCounter = 0;
				state = State::hit2;
			}
			break;
		case State::hit2:
			if (stateCounter > 547) {		// approx 11.4ms
				level = config.initLevel;
				stateCounter = 0;
				state = State::hit3;
			}
			break;
		case State::hit3:
			if (stateCounter > 336) {		// approx 7ms
				level = config.initLevel;
				stateCounter = 0;
				state = State::hit4;
			}
			break;
		default:
			break;
		}

		output += config.unfilteredNoiseLevel * intToFloatMult * static_cast<int32_t>(RNG->DR);		// Add some additional non-filtered noise back in

		level *= (state == State::hit4 ? config.reverbDecay : config.initDecay);

		outputLevel[left]  = velocityScale * (output * level);
		outputLevel[right] = outputLevel[left];

		if (level < 0.00001f && state == State::hit4) {
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
