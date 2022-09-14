#include <Sequencer.h>

Sequencer sequencer;

Sequencer::Sequencer()
{
	// Create simple rock pattern for testing
	sequence[0].info.bars = 2;
	sequence[0].info.beatsPerBar = 16;

	sequence[0].bar[0].beat[0][VoiceManager::kick].index = 0;
	sequence[0].bar[0].beat[0][VoiceManager::kick].level = 127;
	sequence[0].bar[0].beat[0][VoiceManager::hihat].index = 0;
	sequence[0].bar[0].beat[0][VoiceManager::hihat].level = 100;

	sequence[0].bar[0].beat[2][VoiceManager::hihat].index = 0;
	sequence[0].bar[0].beat[2][VoiceManager::hihat].level = 20;

	sequence[0].bar[0].beat[4][VoiceManager::snare].index = 0;
	sequence[0].bar[0].beat[4][VoiceManager::snare].level = 110;
	sequence[0].bar[0].beat[4][VoiceManager::hihat].index = 0;
	sequence[0].bar[0].beat[4][VoiceManager::hihat].level = 100;

	sequence[0].bar[0].beat[6][VoiceManager::hihat].index = 0;
	sequence[0].bar[0].beat[6][VoiceManager::hihat].level = 30;

	sequence[0].bar[0].beat[8][VoiceManager::kick].index = 0;
	sequence[0].bar[0].beat[8][VoiceManager::kick].level = 127;
	sequence[0].bar[0].beat[8][VoiceManager::hihat].index = 0;
	sequence[0].bar[0].beat[8][VoiceManager::hihat].level = 100;

	sequence[0].bar[0].beat[10][VoiceManager::hihat].index = 0;
	sequence[0].bar[0].beat[10][VoiceManager::hihat].level = 10;

	sequence[0].bar[0].beat[12][VoiceManager::snare].index = 0;
	sequence[0].bar[0].beat[12][VoiceManager::snare].level = 120;
	sequence[0].bar[0].beat[12][VoiceManager::hihat].index = 0;
	sequence[0].bar[0].beat[12][VoiceManager::hihat].level = 80;

	sequence[0].bar[0].beat[14][VoiceManager::hihat].index = 0;
	sequence[0].bar[0].beat[14][VoiceManager::hihat].level = 70;


	sequence[0].bar[1].beat[0][VoiceManager::kick].index = 0;
	sequence[0].bar[1].beat[0][VoiceManager::kick].level = 127;
	sequence[0].bar[1].beat[0][VoiceManager::hihat].index = 0;
	sequence[0].bar[1].beat[0][VoiceManager::hihat].level = 100;

	sequence[0].bar[1].beat[2][VoiceManager::hihat].index = 0;
	sequence[0].bar[1].beat[2][VoiceManager::hihat].level = 20;

	sequence[0].bar[1].beat[4][VoiceManager::snare].index = 0;
	sequence[0].bar[1].beat[4][VoiceManager::snare].level = 110;
	sequence[0].bar[1].beat[4][VoiceManager::hihat].index = 0;
	sequence[0].bar[1].beat[4][VoiceManager::hihat].level = 100;

	sequence[0].bar[1].beat[6][VoiceManager::hihat].index = 0;
	sequence[0].bar[1].beat[6][VoiceManager::hihat].level = 30;

	sequence[0].bar[1].beat[8][VoiceManager::kick].index = 0;
	sequence[0].bar[1].beat[8][VoiceManager::kick].level = 127;
	sequence[0].bar[1].beat[8][VoiceManager::hihat].index = 0;
	sequence[0].bar[1].beat[8][VoiceManager::hihat].level = 100;

	sequence[0].bar[1].beat[10][VoiceManager::hihat].index = 0;
	sequence[0].bar[1].beat[10][VoiceManager::hihat].level = 10;

	sequence[0].bar[1].beat[12][VoiceManager::snare].index = 0;
	sequence[0].bar[1].beat[12][VoiceManager::snare].level = 120;
	sequence[0].bar[1].beat[12][VoiceManager::hihat].index = 0;
	sequence[0].bar[1].beat[12][VoiceManager::hihat].level = 80;

	sequence[0].bar[1].beat[14][VoiceManager::hihat].index = 0;
	sequence[0].bar[1].beat[14][VoiceManager::hihat].level = 70;

	sequence[0].bar[1].beat[14][VoiceManager::samplerA].index = 2;
	sequence[0].bar[1].beat[14][VoiceManager::samplerA].level = 70;
}



void Sequencer::StartStop(uint8_t seq)
{
	if (!playing) {
		playing = true;
		activeSequence = seq;
		position = 0;
		currentBar = 0;
		currentBeat = 0;
	} else {
		playing = false;
	}
}


void Sequencer::Play()
{
	if (playing) {
		Sequence& seq = sequence[activeSequence];

		if (currentBar >= seq.info.bars) {
			currentBar = 0;
		}

		// Get tempo
		tempo = 0.9f * tempo + 0.1f * (0.5f * ADC_array[ADC_Tempo]);
		uint32_t beatLen = 40000.0f - tempo;

		if (position == 0) {
			for (uint32_t i = 0; i < VoiceManager::voiceCount; ++i) {
				auto& b = seq.bar[currentBar].beat[currentBeat][i];
				if (b.level > 0) {
					auto& note = voiceManager.noteMapper[i];
					note.drumVoice->Play(note.voiceIndex, b.index, 0, static_cast<float>(b.level) / 127.0f);
				}
			}
		}

		if (++position > beatLen) {
			position = 0;
			if (++currentBeat >= seq.info.beatsPerBar) {
				currentBeat = 0;
				++currentBar;
			}
		}

	}
}


Sequencer::SeqInfo Sequencer::GetSeqInfo(uint8_t seq)
{
	activeSequence = seq;				// Update active sequence so switching sequence in the editor updates playing sequence
	return sequence[seq].info;
}


uint32_t Sequencer::GetBar(uint8_t** buff, uint8_t seq, uint8_t bar)
{
	*buff = (uint8_t*)&(sequence[seq].bar[bar]);
	return sizeof(sequence[seq].bar[bar]);
}


void Sequencer::StoreConfig(uint8_t* buff, uint32_t len, uint8_t seq, uint8_t bar, uint8_t beatsPerBar, uint8_t bars)
{
	sequence[seq].info.bars = bars;
	sequence[seq].info.beatsPerBar = beatsPerBar;
	if (len <= sizeof(sequence[seq].bar[bar])) {
		memcpy(&(sequence[seq].bar[bar]), buff, len);
	}
}
