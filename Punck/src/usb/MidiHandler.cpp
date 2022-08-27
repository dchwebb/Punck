#include "USB.h"
#include "MidiHandler.h"
#include "VoiceManager.h"
#include "config.h"

void MidiHandler::DataIn()
{

}

void MidiHandler::DataOut()
{
	uint8_t* outBuffBytes = reinterpret_cast<uint8_t*>(outBuff);
	// Handle incoming midi command here
	if (outBuffCount == 4) {
		midiEvent(*outBuff);

	} else if (outBuffBytes[1] == 0xF0 && outBuffCount > 3) {		// Sysex
		// sysEx will be padded when supplied by usb - add only actual sysEx message bytes to array
		uint8_t sysExCnt = 2, i = 0;
		for (i = 0; i < sysexMaxSize; ++i) {
			if (outBuffBytes[sysExCnt] == 0xF7) break;
			sysEx[i] = outBuffBytes[sysExCnt++];

			// remove 1 byte padding at the beginning of each 32 bit word
			if (sysExCnt % 4 == 0) {
				++sysExCnt;
			}
		}
		sysExCount = i;
		ProcessSysex();
	}
}


uint32_t MidiHandler::ConstructSysEx(uint8_t* buffer, uint32_t len)
{
	// Constructs a Sysex packet: data split into 4 byte words, each starting with appropriate sysex header byte
	// Bytes in a sysEx commands must have upper nibble = 0 (ie only allowed 0-127 values) so double length and split bytes into nibbles
	len *= 2;
	sysExOut[0] = 0x04;								// 0x4	SysEx starts or continues
	sysExOut[1] = 0xF0;
	uint32_t sysExCnt = 2;
	bool lowerNibble = true;
	for (uint32_t i = 0; i < len; ++i) {
		if (lowerNibble) {
			sysExOut[sysExCnt++] = buffer[i / 2] & 0xF;
		} else {
			sysExOut[sysExCnt++] = buffer[i / 2] >> 4;
		}
		lowerNibble = !lowerNibble;

		// add 1 byte padding at the beginning of each 32 bit word to indicate length of remaining message + 0xF7 termination byte
		if (sysExCnt % 4 == 0) {
			uint32_t rem = len - i;

			if (rem == 3) {
				sysExOut[sysExCnt++] = 0x07;		// 0x7	SysEx ends with following three bytes.
			} else if (rem == 2) {
				sysExOut[sysExCnt++] = 0x06;		// 0x6	SysEx ends with following two bytes.
			} else if (rem == 1) {
				sysExOut[sysExCnt++] = 0x05;		// 0x5	Single-byte System Common Message or SysEx ends with following single byte.
			} else {
				sysExOut[sysExCnt++] = 0x04;		// 0x4	SysEx starts or continues
			}
		}


	}
	sysExOut[sysExCnt++] = 0xF7;
	return sysExCnt;
}

struct TestSysex {
	uint8_t a = 0x11;
	uint8_t b = 0x12;
	uint8_t c = 0x13;
	uint8_t d = 0x14;
	uint8_t e = 0x15;
	uint8_t f = 0x16;
	uint8_t g = 0x17;
	uint8_t h = 0x18;
	uint8_t i = 0x19;
} testSysEx;

void MidiHandler::ProcessSysex()
{

//	tx.Code = 0x03;
//	tx.db0 = 0xF0;
//	tx.db1 = sysExCount;
//	tx.db2 = 0xF7;
//	usb->SendData((uint8_t*) &tx, 4, inEP);

	enum sysExCommands {VoiceConfig = 0x1C};

	// Check if SysEx contains read config command
	if (sysEx[0] == VoiceConfig && sysEx[1] < VoiceManager::voiceCount) {
		uint32_t bytes = voiceManager.noteMapper[sysEx[1]].drumVoice->SerialiseConfig(config.configBuffer);
		uint32_t len = ConstructSysEx(config.configBuffer, bytes);
		len = ((len + 3) / 4) * 4;			// round up output size to multiple of 4
		usb->SendData(sysExOut, len, inEP);

	}
/*
	// Store float into test variable
	float testFloat = 1.234567f;
	float* ptr = (float*)&testSysEx;
	*ptr = testFloat;

	uint32_t len = ConstructSysEx((uint8_t*)&testSysEx, 6);
	len = ((len + 3) / 4) * 4;			// round up output size to multiple of 4
	usb->SendData(sysExOut, len, inEP);
	*/
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



