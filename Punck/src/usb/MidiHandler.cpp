#include "USB.h"
#include "MidiHandler.h"
#include "VoiceManager.h"
#include "config.h"
#include "sequencer.h"


void MidiHandler::DataIn()
{

}

void MidiHandler::DataOut()
{
	// Handle incoming midi command here
	const uint8_t* outBuffBytes = reinterpret_cast<const uint8_t*>(outBuff);

	if (!partialSysEx && outBuffCount == 4) {
		midiEvent(*outBuff);

	} else if (partialSysEx || (outBuffBytes[1] == 0xF0 && outBuffCount > 3)) {		// Sysex
		// sysEx will be padded when supplied by usb - add only actual sysEx message bytes to array
		uint16_t sysExCnt = partialSysEx ? 1 : 2;		// If continuing a long sysex command only ignore first (size) byte

		uint16_t i;
		for (i = partialSysEx ? sysExCount : 0; i < sysexMaxSize; ++i) {
			if (outBuffBytes[sysExCnt] == 0xF7) {
				partialSysEx = false;
				break;
			}
			if (sysExCnt >= outBuffCount) {				// Long SysEx command will be received in multiple packets
				partialSysEx = true;
				break;
			}

			sysEx[i] = outBuffBytes[sysExCnt++];

			// remove 1 byte padding at the beginning of each 32 bit word
			if (sysExCnt % 4 == 0) {
				++sysExCnt;
			}
		}
		sysExCount = i;
		if (!partialSysEx) {
			ProcessSysex();
		}
	}
}


uint32_t MidiHandler::ConstructSysEx(const uint8_t* dataBuffer, uint32_t dataLen, const uint8_t* headerBuffer, const uint32_t headerLen, const bool noSplit)
{
	// Constructs a Sysex packet: data split into 4 byte words, each starting with appropriate sysex header byte
	// Bytes in sysEx commands must have upper nibble = 0 (ie only 0-127 values) so double length and split bytes into nibbles (unless noSplit specified)

	if (!noSplit) {
		dataLen *= 2;
	}
	uint32_t pos = 0;

	sysExOut[pos++] = 0x04;					// 0x4	SysEx starts or continues
	sysExOut[pos++] = 0xF0;

	// header must be 7 bit values so no need to split
	for (uint32_t i = 0; i < headerLen; ++i) {
		sysExOut[pos++] = headerBuffer[i];
		if (pos % 4 == 0) {
			sysExOut[pos++] = 0x04;			// 0x4	SysEx starts or continues
		}
	}

	bool lowerNibble = true;
	for (uint32_t i = 0; i < dataLen; ++i) {
		if (noSplit) {
			sysExOut[pos++] = dataBuffer[i];
		} else {
			if (lowerNibble) {
				sysExOut[pos++] = dataBuffer[i / 2] & 0xF;
			} else {
				sysExOut[pos++] = dataBuffer[i / 2] >> 4;
			}
			lowerNibble = !lowerNibble;
		}

		// add 1 byte padding at the beginning of each 32 bit word to indicate length of remaining message + 0xF7 termination byte
		if (pos % 4 == 0) {
			const uint32_t rem = dataLen - i;

			if (rem == 3) {
				sysExOut[pos++] = 0x07;		// 0x7	SysEx ends with following three bytes.
			} else if (rem == 2) {
				sysExOut[pos++] = 0x06;		// 0x6	SysEx ends with following two bytes.
			} else if (rem == 1) {
				sysExOut[pos++] = 0x05;		// 0x5	Single-byte System Common Message or SysEx ends with following single byte.
			} else {
				sysExOut[pos++] = 0x04;		// 0x4	SysEx starts or continues
			}
		}
	}
	sysExOut[pos++] = 0xF7;

	if (pos > sysexMaxSize) {
		int susp = 1;
	}

	pos = ((pos + 3) / 4) * 4;				// round up output size to multiple of 4
	return pos;
}


uint32_t MidiHandler::ReadCfgSysEx(uint8_t headerLength)
{
	// Converts a Configuration encoded Sysex packet to a regular byte array (two bytes are header)
	// Data split into 4 byte words, each starting with appropriate sysEx header byte
	bool lowerNibble = true;
	for (uint32_t i = headerLength; i < sysExCount; ++i) {
		uint32_t idx = (i - headerLength) / 2;
		if (lowerNibble) {
			config.configBuffer[idx] = sysEx[i];
		} else {
			config.configBuffer[idx] += sysEx[i] << 4;
		}
		lowerNibble = !lowerNibble;
	}
	return (sysExCount - headerLength) / 2;
}


void MidiHandler::ProcessSysex()
{
	enum sysExCommands {StartStopSeq = 0x1A, GetSequence = 0x1B, SetSequence = 0x2B, GetVoiceConfig = 0x1C, SetVoiceConfig = 0x2C, GetSamples = 0x1D,
		GetStatus = 0x1E, GetADC = 0x1F, SetADC = 0x2F};

	// Check if SysEx contains read config command
	switch (sysEx[0]) {
		case GetVoiceConfig:
			if (sysEx[1] < VoiceManager::Voice::count) {
				VoiceManager::Voice voice = (VoiceManager::Voice)sysEx[1];

				// Insert header data
				const uint8_t cfgHeader[2] = {GetVoiceConfig, voice};

				uint8_t* cfgBuffer = nullptr;
				NoteMapper& vm = voiceManager.noteMapper[voice];
				const uint32_t bytes = vm.drumVoice->SerialiseConfig(&cfgBuffer, vm.voiceIndex);
				const uint32_t len = ConstructSysEx(cfgBuffer, bytes, cfgHeader, 2, split);

				usb->SendData(sysExOut, len, inEP);
			}
			break;

		case SetVoiceConfig:
			if (sysEx[1] < VoiceManager::Voice::count) {
				const uint32_t bytes = ReadCfgSysEx(2);
				voiceManager.noteMapper[sysEx[1]].drumVoice->StoreConfig(config.configBuffer, bytes);
			}
			break;

		case StartStopSeq:
			sequencer.StartStop(sysEx[1]);
			break;

		case GetStatus:
		{
			// Get playing status from sequence
			config.configBuffer[0] = GetStatus;
			config.configBuffer[1] = sequencer.playing ? 1 : 0;					// playing
			config.configBuffer[2] = sequencer.activeSequence; 					// active sequence
			config.configBuffer[3] = sequencer.currentBar;     					// current bar
			config.configBuffer[4] = sequencer.currentBeat;    					// current beat

			const uint32_t len = ConstructSysEx(config.configBuffer, 5, nullptr, 0, noSplit);
			usb->SendData(sysExOut, len, inEP);
			break;
		}

		case GetSequence:
		{
			uint8_t seq = sysEx[1];							// if passed 127 then requesting currently active sequence
			const uint8_t bar = sysEx[2];
			if (seq == getActiveSequence) {
				seq = sequencer.activeSequence;
			}
			auto seqInfo = sequencer.GetSeqInfo(seq);

			// Insert header data
			config.configBuffer[0] = GetSequence;
			config.configBuffer[1] = seq;					// sequence
			config.configBuffer[2] = seqInfo.beatsPerBar;	// beats per bar
			config.configBuffer[3] = seqInfo.bars;			// bars
			config.configBuffer[4] = bar;					// bar number

			uint8_t* cfgBuffer = nullptr;
			const uint32_t bytes = sequencer.GetBar(&cfgBuffer, seq, bar);
			const uint32_t len = ConstructSysEx(cfgBuffer, bytes, config.configBuffer, 5, noSplit);
			usb->SendData(sysExOut, len, inEP);
			break;
		}

		case SetSequence:
		{
			// Header information
			const uint32_t seq = sysEx[1];					// sequence
			const uint32_t beatsPerBar = sysEx[2];			// beats per bar
			const uint32_t bars = sysEx[3];					// bars
			const uint32_t bar = sysEx[4];					// bar number

			sequencer.StoreConfig(sysEx + 5, sysExCount - 5, seq, bar, beatsPerBar, bars);
			break;
		}

		case GetSamples:
		{
			const uint8_t samplePlayer = sysEx[1];
			const uint8_t cfgHeader[2] = {GetSamples, samplePlayer};		// Insert header data

			uint8_t* cfgBuffer = nullptr;
			const uint32_t bytes = voiceManager.samples.SerialiseSampleNames(&cfgBuffer, samplePlayer);;
			const uint32_t len = ConstructSysEx(cfgBuffer, bytes, cfgHeader, 2, noSplit);

			usb->SendData(sysExOut, len, inEP);
			break;
		}

		case GetADC:
			{
				// Get current (virtual) ADC settings
				config.configBuffer[0] = GetADC;

				const uint32_t len = ConstructSysEx((uint8_t*)ADC_array, sizeof(ADC_array), config.configBuffer, 1, split);
				usb->SendData(sysExOut, len, inEP);
			}
			break;

		case SetADC:
			{
				const uint32_t bytes = ReadCfgSysEx(1);
				memcpy((uint8_t*)ADC_array, config.configBuffer, bytes);
			}
			break;
	}



}


void MidiHandler::midiEvent(const uint32_t data)
{
	auto midiData = MidiData(data);
	MidiNote midiNote(midiData.db1, midiData.db2);

	switch (midiData.msg) {
	case NoteOff:
		break;

	case NoteOn:
		voiceManager.NoteOn(midiNote);
		break;

	case PitchBend:
		pitchBend = static_cast<uint32_t>(midiData.db1) + (midiData.db2 << 7);
		break;
	}

}


void MidiHandler::serialHandler(uint32_t data)
{
	Queue[QueueWrite] = data;
	QueueSize++;
	QueueWrite = (QueueWrite + 1) % MIDIQUEUESIZE;

	MIDIType type = static_cast<MIDIType>(Queue[QueueRead] >> 4);
	uint8_t channel = Queue[QueueRead] & 0x0F;

	//NoteOn = 0x9, NoteOff = 0x8, PolyPressure = 0xA, ControlChange = 0xB, ProgramChange = 0xC, ChannelPressure = 0xD, PitchBend = 0xE, System = 0xF
	while ((QueueSize > 2 && (type == NoteOn || type == NoteOff || type == PolyPressure ||  type == ControlChange ||  type == PitchBend)) ||
			(QueueSize > 1 && (type == ProgramChange || type == ChannelPressure))) {

		MidiData event;
		event.chn = channel;
		event.msg = (uint8_t)type;

		QueueInc();
		event.db1 = Queue[QueueRead];
		QueueInc();
		if (type == ProgramChange || type == ChannelPressure) {
			event.db2 = 0;
		} else {
			event.db2 = Queue[QueueRead];
			QueueInc();
		}

		midiEvent(event.data);

		type = static_cast<MIDIType>(Queue[QueueRead] >> 4);
		channel = Queue[QueueRead] & 0x0F;
	}

	// Clock
	if (QueueSize > 0 && Queue[QueueRead] == 0xF8) {
		midiEvent(0xF800);
		QueueInc();
	}

	//	handle unknown data in queue
	if (QueueSize > 2 && type != 0x9 && type != 0x8 && type != 0xD && type != 0xE) {
		QueueInc();
	}
}


inline void MidiHandler::QueueInc() {
	QueueSize--;
	QueueRead = (QueueRead + 1) % MIDIQUEUESIZE;
}




void MidiHandler::ClassSetup(usbRequest& req)
{
}


void MidiHandler::ClassSetupData(usbRequest& req, const uint8_t* data)
{
}


/*
Byte 1									|	Byte2		|	Byte 3		|	Byte 4
Cable Number | Code Index Number (CIN)	|	MIDI_0		|	MIDI_1		|	MIDI_2

CIN		MIDI_x Size Description
0x0		1, 2 or 3	Miscellaneous function codes. Reserved for future extensions.
0x1		1, 2 or 3	Cable events. Reserved for future expansion.
0x2		2			Two-byte System Common messages like MTC, SongSelect, etc.
0x3		3			Three-byte System Common messages like SPP, etc.
0x4		3			SysEx starts or continues
0x5		1			Single-byte System Common Message or SysEx ends with following single byte.
0x6		2			SysEx ends with following two bytes.
0x7		3			SysEx ends with following three bytes.
0x8		3			Note-off
0x9		3			Note-on
0xA		3			Poly-KeyPress
0xB		3			Control Change
0xC		2			Program Change
0xD		2			Channel Pressure
0xE		3			PitchBend Change
0xF		1			Single Byte
*/



