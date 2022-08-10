#include "MidiHandler.h"
#include "NoteHandler.h"

void MidiHandler::DataIn()
{

}

void MidiHandler::DataOut()
{
	// Handle incoming midi command here
	if (outBuffCount == 4) {
		midiEvent(*outBuff);

	} else if (outBuff[1] == 0xF0 && outBuffCount > 3) {		// Sysex
		// sysEx will be padded when supplied by usb - add only actual sysEx message bytes to array
		uint8_t sysExCnt = 2, i = 0;
		for (i = 0; i < 32; ++i) {
			if (outBuff[sysExCnt] == 0xF7) break;
			sysEx[i] = outBuff[sysExCnt++];

			// remove 1 byte padding at the beginning of each 32bit word
			if (sysExCnt % 4 == 0) {
				++sysExCnt;
			}
		}
		sysExCount = i;
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
		noteHandler.NoteOn(midiNote);
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






