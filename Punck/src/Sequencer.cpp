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



	// Create swingpattern for testing
	sequence[1].info.bars = 1;
	sequence[1].info.beatsPerBar = 24;

	sequence[1].bar[0].beat[0][VoiceManager::kick].level = 127;
	sequence[1].bar[0].beat[0][VoiceManager::hihat].level = 40;

	sequence[1].bar[0].beat[2][VoiceManager::hihat].level = 20;

	sequence[1].bar[0].beat[3][VoiceManager::snare].level = 110;

	sequence[1].bar[0].beat[4][VoiceManager::hihat].level = 40;
	sequence[1].bar[0].beat[4][VoiceManager::kick].level = 100;

	sequence[1].bar[0].beat[6][VoiceManager::snare].level = 100;
	sequence[1].bar[0].beat[6][VoiceManager::hihat].level = 60;

	sequence[1].bar[0].beat[8][VoiceManager::hihat].level = 40;

	sequence[1].bar[0].beat[10][VoiceManager::kick].level = 127;
	sequence[1].bar[0].beat[10][VoiceManager::hihat].level = 100;

	sequence[1].bar[0].beat[12][VoiceManager::kick].level = 127;
	sequence[1].bar[0].beat[12][VoiceManager::hihat].level = 50;

	sequence[1].bar[0].beat[14][VoiceManager::hihat].level = 10;

	sequence[1].bar[0].beat[15][VoiceManager::snare].level = 100;

	sequence[1].bar[0].beat[16][VoiceManager::kick].level = 100;
	sequence[1].bar[0].beat[16][VoiceManager::hihat].level = 40;

	sequence[1].bar[0].beat[18][VoiceManager::snare].level = 120;
	sequence[1].bar[0].beat[18][VoiceManager::hihat].level = 80;

	sequence[1].bar[0].beat[20][VoiceManager::hihat].level = 70;

	sequence[1].bar[0].beat[22][VoiceManager::kick].level = 127;
	sequence[1].bar[0].beat[22][VoiceManager::hihat].level = 50;
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
		tempo = 0.9f * tempo + 0.1f * (0.25f * ADC_array[ADC_Tempo]);
		uint32_t beatLen = (18000.0f - tempo) * (16.0f / (float)seq.info.beatsPerBar);

		if (position == 0) {
			for (uint32_t i = 0; i < VoiceManager::Voice::count; ++i) {
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


void Sequencer::ChangeSequence(uint8_t seq)
{
	// Change playing sequence to synchronise time when changing between different beats per bar
	if (seq != activeSequence) {
		SeqInfo& oldSeq = sequence[activeSequence].info;
		SeqInfo& newSeq = sequence[seq].info;

		if (playing && oldSeq.beatsPerBar != newSeq.beatsPerBar) {

			// Normalise position
			float oldbeatLen = (18000.0f - tempo) * (16.0f / (float)oldSeq.beatsPerBar);
			float oldPos = (float)position + (oldbeatLen * currentBeat);
			float newbeatLen = (18000.0f - tempo) * (16.0f / (float)newSeq.beatsPerBar);

			currentBeat = std::floor(oldPos / newbeatLen);
			position = oldPos - (currentBeat * newbeatLen);
		}
		activeSequence = seq;
	}
}


Sequencer::SeqInfo Sequencer::GetSeqInfo(uint8_t seq)
{
	ChangeSequence(seq);				// Update active sequence so switching sequence in the editor updates playing sequence
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
