#pragma once

#include "initialisation.h"
#include "USBHandler.h"

class USB;

class MidiHandler : public USBHandler {
public:
	MidiHandler(USB* usb, uint8_t inEP, uint8_t outEP, int8_t interface) : USBHandler(usb, inEP, outEP, interface) {
		outBuff = xfer_buff;
	}

	void DataIn() override;
	void DataOut() override;
	void ActivateEP() override;
	void ClassSetup(usbRequest& req) override;
	void ClassSetupData(usbRequest& req, const uint8_t* data) override;
	uint32_t GetInterfaceDescriptor(const uint8_t** buffer) override;

	void serialHandler(uint32_t data);

	enum MIDIType {Unknown = 0, NoteOn = 0x9, NoteOff = 0x8, PolyPressure = 0xA, ControlChange = 0xB,
		ProgramChange = 0xC, ChannelPressure = 0xD, PitchBend = 0xE, System = 0xF };

	struct MidiNote {
		MidiNote(uint8_t n, uint8_t v) : noteValue(n), velocity(v) {};

		uint8_t noteValue;		// MIDI note value
		uint8_t velocity;
	};

	static const uint8_t Descriptor[];
	static constexpr uint8_t MidiClassDescSize = 50;		// size of just the MIDI class definition (excluding interface descriptors)

	uint16_t pitchBend = 8192;								// Pitchbend amount in raw format (0 - 16384)
	const float pitchBendSemiTones = 12.0f;					// Number of semitones for a full pitchbend

private:
	enum sysExCommands {StartStopSeq = 0x1A, GetSequence = 0x1B, SetSequence = 0x1C, GetVoiceConfig = 0x1D, SetVoiceConfig = 0x1E, GetSamples = 0x1F,
		GetStatus = 0x20, SaveConfig = 0x21, GetReverbConfig = 0x22, SetReverbConfig = 0x23};

	void midiEvent(const uint32_t data);
	void QueueInc();
	void ProcessSysex();
	uint32_t ConstructSysEx(const uint8_t* buffer, uint32_t len, const uint8_t* headerBuffer, const uint32_t headerLen, const bool noSplit);
	uint32_t ReadCfgSysEx(uint8_t headerLength);

	static constexpr bool noSplit = true;
	static constexpr bool split = false;

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
		// Used primarily for transferring configuration data to the editor
		struct {
			uint8_t Code;
			uint8_t db0;
			uint8_t cfgChannelOrOutput : 4;	// holds channel of config data being transferred.	Byte position: ----dddd
			uint8_t configType : 4;			// holds type of config data being transferred.		Byte position: dddd----
			uint8_t configValue;			// value of note number or controller configured
		};
	};

	static constexpr uint8_t SerialQueueSize = 50;
	uint8_t Queue[SerialQueueSize];			// hold incoming serial MIDI bytes
	uint8_t QueueRead = 0;
	uint8_t QueueWrite = 0;
	uint8_t QueueSize = 0;

	constexpr static uint32_t sysexMaxSize = 512;
	uint8_t sysEx[sysexMaxSize];
	uint32_t sysExCount = 0;
	bool partialSysEx;

	MidiData tx;
	uint8_t sysExOut[sysexMaxSize];



};
