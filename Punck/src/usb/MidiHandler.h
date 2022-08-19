#pragma once

#include "initialisation.h"
#include "USBHandler.h"
#define MIDIQUEUESIZE 50

class USB;

class MidiHandler : public USBHandler {
public:
	MidiHandler(USB* usb, uint8_t inEP, uint8_t outEP, int8_t interface) : USBHandler(usb, inEP, outEP, interface) {
		outBuff = xfer_buff;
	}

	void DataIn() override;
	void DataOut() override;
	void ClassSetup(usbRequest& req) override;
	void ClassSetupData(usbRequest& req, const uint8_t* data) override;
	void serialHandler(uint32_t data);

	enum MIDIType {Unknown = 0, NoteOn = 0x9, NoteOff = 0x8, PolyPressure = 0xA, ControlChange = 0xB, ProgramChange = 0xC, ChannelPressure = 0xD, PitchBend = 0xE, System = 0xF };

	struct MidiNote {
		MidiNote(uint8_t n, uint8_t v) : noteValue(n), velocity(v) {};

		uint8_t noteValue;		// MIDI note value
		uint8_t velocity;
	};

	uint16_t pitchBend = 8192;								// Pitchbend amount in raw format (0 - 16384)
	const float pitchBendSemiTones = 12.0f;					// Number of semitones for a full pitchbend

private:
	void midiEvent(const uint32_t data);
	void QueueInc();

	uint32_t xfer_buff[64];									// OUT Data filled in RxLevel Interrupt

	// Struct for holding incoming USB MIDI data
	union MidiData {
		MidiData(uint32_t d) : data(d) {};
		MidiData()  {};

		uint32_t data;
		struct {
			uint8_t CIN : 4;
			uint8_t cable : 4;
			uint8_t chn : 4;
			uint8_t msg : 4;
			uint8_t db1;
			uint8_t db2;
		};
	};

	uint8_t Queue[MIDIQUEUESIZE];			// hold incoming serial MIDI bytes
	uint8_t QueueRead = 0;
	uint8_t QueueWrite = 0;
	uint8_t QueueSize = 0;

	uint8_t sysEx[32];
	uint8_t sysExCount = 0;
};