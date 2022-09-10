#include <Sequencer.h>

Sequencer sequencer;

Sequencer::Sequencer()
{
	// Create simple rock pattern for testing
	bar.beat[0][VoiceManager::kick].index = 0;
	bar.beat[0][VoiceManager::kick].level = 127;
	bar.beat[0][VoiceManager::hihat].index = 0;
	bar.beat[0][VoiceManager::hihat].level = 100;

	bar.beat[2][VoiceManager::hihat].index = 0;
	bar.beat[2][VoiceManager::hihat].level = 20;

	bar.beat[4][VoiceManager::snare].index = 0;
	bar.beat[4][VoiceManager::snare].level = 110;
	bar.beat[4][VoiceManager::hihat].index = 0;
	bar.beat[4][VoiceManager::hihat].level = 100;

	bar.beat[6][VoiceManager::hihat].index = 0;
	bar.beat[6][VoiceManager::hihat].level = 30;

	bar.beat[8][VoiceManager::kick].index = 0;
	bar.beat[8][VoiceManager::kick].level = 127;
	bar.beat[8][VoiceManager::hihat].index = 0;
	bar.beat[8][VoiceManager::hihat].level = 100;

	bar.beat[10][VoiceManager::hihat].index = 0;
	bar.beat[10][VoiceManager::hihat].level = 10;

	bar.beat[12][VoiceManager::snare].index = 0;
	bar.beat[12][VoiceManager::snare].level = 120;
	bar.beat[12][VoiceManager::hihat].index = 0;
	bar.beat[12][VoiceManager::hihat].level = 80;

	bar.beat[14][VoiceManager::hihat].index = 0;
	bar.beat[14][VoiceManager::hihat].level = 70;

	bar.beat[14][VoiceManager::samplerA].index = 2;
	bar.beat[14][VoiceManager::samplerA].level = 70;
}


void Sequencer::Start()
{
	if (!playing) {
		playing = true;
		position = 0;
		beatLen = 24000;
		currentBeat = 0;
	} else {
		playing = false;
	}
}


void Sequencer::Play()
{
	if (playing) {
		// Get tempo
		tempo = 0.9f * tempo + 0.1f * (0.5f * ADC_array[ADC_Tempo]);
		beatLen = 40000.0f - tempo;

		if (position == 0) {
			for (uint32_t i = 0; i < VoiceManager::voiceCount; ++i) {
				auto& b = bar.beat[currentBeat][i];
				if (b.level > 0) {
					auto& note = voiceManager.noteMapper[i];
					note.drumVoice->Play(note.voiceIndex, b.index, 0, static_cast<float>(b.level) / 127.0f);
				}
			}
		}

		if (++position > beatLen) {
			position = 0;
			if (++currentBeat >= beatsPerBar) {
				currentBeat = 0;
			}
		}

	}
}


uint32_t Sequencer::SerialiseConfig(uint8_t** buff)
{
	*buff = (uint8_t*)&bar;
	//memcpy(buff, &bar, sizeof(bar));
	return sizeof(bar);
}


void Sequencer::ReadConfig(uint8_t* buff, uint32_t len)
{
	if (len <= sizeof(bar)) {
		memcpy(&bar, buff, len);
	}
}
