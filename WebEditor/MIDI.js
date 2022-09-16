if (navigator.requestMIDIAccess) {
    console.log('This browser supports WebMIDI');
} else {
    console.log('WebMIDI is not supported in this browser.');
}

var midi = null;           // global MIDIAccess object
var output = null;
var requestNo = 0;         // Config is retrieved in chunks - this holds index of current chunk of data being recovered

// enum from c++ code to match voice
var voiceEnum = {
    Kick: 0, Snare: 1, HiHat: 2, Sampler_A: 3, Sampler_B: 4
};

var requestEnum = {
    StartStop: 0x1A, GetSequence: 0x1B, SetSequence: 0x2B, GetVoiceConfig: 0x1C, SetVoiceConfig: 0x2C, GetSamples: 0x1D
};


var kickSettings = [
	{name: 'Ramp 1 Inc', value: 'kiRamp1Inc'},
	{name: 'Ramp 2 Inc', value: 'kiRamp2Inc'},
	{name: 'Ramp 3 Inc', value: 'kiRamp3Inc'},

	{name: 'Fast Sine Inc', value: 'kiFastSinInc'},
	{name: 'Slow Sine Inc', value: 'kiInitSlowSinInc'},
	
	{name: 'sineSlowDownRate', value: 'kiSineSlowDownRate'},
];

var snareSettings = [
	{name: 'Noise level', value: 'snNoiseInitLevel'},
	{name: 'Noise Decay', value: 'snNoiseDecay'},

	{name: 'Base Frequency', value: 'snBaseFreq'},
	{name: 'Partial Decay', value: 'snPartialDecay'},
	
	{name: 'Partial 0 Level', value: 'snPartial0Level'},
	{name: 'Partial 1 Level', value: 'snPartial1Level'},
	{name: 'Partial 2 Level', value: 'snPartial2Level'},
	
	{name: 'Partial 0 Freq Offset', value: 'snFreq0'},
	{name: 'Partial 1 Freq Offset', value: 'snFreq1'},
	{name: 'Partial 2 Freq Offset', value: 'snFreq2'},
];

var hihatSettings = [
	{name: 'Attack', value: 'hhAttackInc'}, 
	{name: 'Decay', value: 'hhDecay'},
	{name: 'HP Initial Cutoff', value: 'hhHPInitCutoff'},
	{name: 'HP Final Cutoff', value: 'hhHPFinalCutoff'},
	{name: 'Noise level', value: 'hhNoiseInitLevel'},
	{name: 'Noise Decay', value: 'hhNoiseDecay'},

	{name: 'Partial 0 Level', value: 'hhPLevel0'},
	{name: 'Partial 1 Level', value: 'hhPLevel1'},
	{name: 'Partial 2 Level', value: 'hhPLevel2'},
	{name: 'Partial 3 Level', value: 'hhPLevel3'},
	{name: 'Partial 4 Level', value: 'hhPLevel4'},
	{name: 'Partial 5 Level', value: 'hhPLevel5'},

	{name: 'Partial 0 Frequency', value: 'hhPFreq0'},
	{name: 'Partial 1 Frequency', value: 'hhPFreq1'},
	{name: 'Partial 2 Frequency', value: 'hhPFreq2'},
	{name: 'Partial 3 Frequency', value: 'hhPFreq3'},
	{name: 'Partial 4 Frequency', value: 'hhPFreq4'},
	{name: 'Partial 5 Frequency', value: 'hhPFreq5'},
];


var variationPicker = [
	{voice: 'Sampler_A', picker: 'samplePicker0'},
	{voice: 'Sampler_B', picker: 'samplePicker1'},
];


var drumSettings = [
	{heading: "Kick Settings", id: voiceEnum.Kick, settings: kickSettings},
	{heading: "Snare Settings", id: voiceEnum.Snare, settings: snareSettings},
	{heading: "Hihat Settings", id: voiceEnum.HiHat, settings: hihatSettings}
]

// Settings related to drum sequence editing
var sequenceSettings = {seq: 0,	beatsPerBar: 16, bars: 2, bar: 0};
var selectedBeat = "";
var beatOptions = [16, 24];
var maxBeatsPerBar = 24;


function PrintMessage(message, received)
{
	// Print contents of payload to console
	var stringData = "";
	for (i = 0; i < message.length; ++i) {
		stringData += message[i].toString(16) + " ";
	}
	console.log((received ? "Received " : "Sent ") + message.length + ": " + stringData);
}


function SeqStart()
{
	// Start or stop playing the sequence
	var message = new Uint8Array(4);
	message[0] = 0xF0;
	message[1] = requestEnum.StartStop;
	message[2] = sequenceSettings.seq;
	message[3] = 0xF7;

	// Print contents of payload to console
	PrintMessage(message);

	output.send(message);
}


function UpdateDrum(bar, seqStructureChanged)
{
	// Convert drum sequence to sysex information
	var ve = Object.keys(voiceEnum).length;
	var seqLen = maxBeatsPerBar * ve * 2;

	var message = new Uint8Array(7 + seqLen);
	message[0] = 0xF0;
	message[1] = requestEnum.SetSequence;
	message[2] = sequenceSettings.seq;
	message[3] = sequenceSettings.beatsPerBar;
	message[4] = sequenceSettings.bars;
	message[5] = bar;
	var msgPos = 6;

	for (var b = 0; b < maxBeatsPerBar; b++) {
		for (let v in voiceEnum) {
			var beatElement = document.getElementById(bar + v + b);
			if (beatElement != null) {
				message[msgPos++] = parseInt(beatElement.getAttribute("level"));
				message[msgPos++] = parseInt(beatElement.getAttribute("index"));
			} else {
				message[msgPos++] = 0;		// Beat will not exist if beatsPerBar is < maxBeatsPerBar - blank values
				message[msgPos++] = 0;
			}
		}
	}
	message[msgPos] = 0xF7;

	PrintMessage(message);			// Print contents of payload to console
	output.send(message);

	if (seqStructureChanged) {
		RefreshSequence(sequenceSettings.seq);
	}
}


window.onload = afterLoad;
function afterLoad() 
{
	// Build button bar
	var html = '<div style="padding: 2px">Drum Sequence:</div> <div>';
	for (var s = 0; s < 6; ++s) {
		html += `<button id="btnSeq${s}" class="topcoat-button-bar__button--large" onclick="RefreshSequence(${s})" >${s + 1}</button> `;
	}
	html += `&nbsp;&nbsp;<button id="btnEditConfig" class="topcoat-button-bar__button--large" onclick="RefreshConfig();" style="background-color: rgb(74, 77, 78)">Drum Settings</button></div>`;
	document.getElementById("buttonBar").innerHTML = html;
	ClearButtonBar();
	
    navigator.requestMIDIAccess({ sysex: true }).then(onMIDISuccess, onMIDIFailure);
}


function ClearButtonBar()
{
	// Reset all button bar colours so selected button/tab can be lightened
	for (var s = 0; s < 6; ++s) {
		document.getElementById(`btnSeq${s}`).style.backgroundColor = "rgb(74, 77, 78)";
	}
	document.getElementById(`btnEditConfig`).style.backgroundColor = "rgb(74, 77, 78)";
}


function BuildConfigHtml()
{
	// Build html lists of settings for each drum voice
	var html = '<div style="display: grid; grid-template-columns: 250px 100px; padding: 10px; border: 1px solid rgba(0, 0, 0, 0.8);">';
	for (var i = 0; i < drumSettings.length; ++i) {
		html += `<div id="settingsBlock${i}" style="grid-column: 1 / 3;  padding: 10px" onclick="ShowSetting();">${drumSettings[i].heading}</div>`;
		for (var s = 0; s < drumSettings[i].settings.length; s++) {
	 		html += `<div class="grid-container3">${drumSettings[i].settings[s].name}</div>` +
					`<div class="grid-container3"><input type="text" id="${drumSettings[i].settings[s].value}" onchange="updateConfig(${i});"></div>`;
		}
	}
	html += '</div>'

	document.getElementById("drumEditor").innerHTML = html;

	ClearButtonBar();
	document.getElementById(`btnEditConfig`).style.backgroundColor = "#737373";
}



function BeatGotFocus(button)
{
	// Clear border of previously selected note
	if (selectedBeat != "" && document.getElementById(selectedBeat) != null) {
		document.getElementById(selectedBeat).style.border = "";
	}

	// Set appearance and level of
	selectedBeat = button;
	document.getElementById(button).style.border = "3px solid #288edf";
	if (document.getElementById(button).getAttribute("level") > 0) {
		document.getElementById("noteLevel").value = document.getElementById(button).getAttribute("level");
	}

	// FIXME - automatically get variations for all voice types
	for (i = 0; i < variationPicker.length; i++) {
		var picker = document.getElementById(variationPicker[i].picker);
		var currentPicker = button.includes(variationPicker[i].voice);
		picker.style.display = currentPicker ? "block" : "none";

		// If the drum beat is active update the picker
		if (currentPicker && document.getElementById(button).getAttribute("level") > 0) {
			picker.value = document.getElementById(button).getAttribute("index");
		}
	}
	
}

function ActivePicker(button)
{
	// Return the variation picker associated with the button
	for (i = 0; i < variationPicker.length; i++) {
		var picker = document.getElementById(variationPicker[i].picker);
		if (button.includes(variationPicker[i].voice)) {
			return picker;
		}
	}
}

function DrumClicked(bar, note)
{
	noteLevel = parseInt(document.getElementById(bar + note).getAttribute("level"));
	noteIndex = parseInt(document.getElementById(bar + note).getAttribute("index"));
	if (noteLevel > 0) {
		// If mode is click to add/delete set level to 0
		if (document.querySelector('input[name="editMode"]:checked').id == "editModeDelete") {
			document.getElementById(bar + note).setAttribute("level", "0");
			document.getElementById(bar + note).style.backgroundColor = "rgb(74, 77, 78)";
		}
	} else {
		document.getElementById(bar + note).setAttribute("level", document.getElementById("noteLevel").value);
		var activePicker = ActivePicker(bar + note);
		document.getElementById(bar + note).setAttribute("index", activePicker ? activePicker.value : 0);
		document.getElementById(bar + note).style.backgroundColor = "rgb(236, 81, 78)";
	}

	BeatGotFocus(bar + note);					// Clear border of selected note and set new border
	UpdateDrum(bar);
}


function DrumLevelChanged()
{
	if (selectedBeat != "") {
		document.getElementById(selectedBeat).setAttribute("level", document.getElementById("noteLevel").value);
		UpdateDrum(parseInt(selectedBeat.substring(0, 1)));
	}
}


function SampleChanged(picker)
{
	if (selectedBeat != "") {
		document.getElementById(selectedBeat).setAttribute("index", document.getElementById(picker).value);
		UpdateDrum(parseInt(selectedBeat.substring(0, 1)));
	}
}


function SequenceChanged()
{
	// check if editing sequence settings
	var newBars = document.querySelector('input[name="barCount"]:checked').id.substring(8);
	var newBeats = document.querySelector('input[name="beatsPerBar"]:checked').id.substring(11);
	if (sequenceSettings.bars != newBars || sequenceSettings.beatsPerBar != newBeats) {
		seqStructureChanged = true;
		sequenceSettings.bars = newBars;
		sequenceSettings.beatsPerBar = newBeats;
	}
	UpdateDrum(0, true);			// inform update method that a structural change has occurred
}



function BuildSequenceHtml()
{
	// Build drum sequence editor html
	
	// Sequence header html (number of bars and beats per bar)
	var barHtml = '';
	for (var bars = 1; bars < 5; ++bars) {
		barHtml += `<input type="radio" id="barCount${bars}" name="barCount" onclick="SequenceChanged();"><div class="topcoat-radio-button__checkmark"></div> 
					<label class="topcoat-radio-button"> ${bars} </label> `;
	}
	var beatsHtml = '';
	for (const beats of beatOptions) {
		beatsHtml += `<input type="radio" id="beatsPerBar${beats}" name="beatsPerBar" onclick="SequenceChanged();"><div class="topcoat-radio-button__checkmark"></div> 
					  <label class="topcoat-radio-button"> ${beats} </label> `;
	}

	var html = 
	`<div style="display: grid; grid-template-columns: 140px 200px 100px 150px; padding: 10px;">
		<div>Number of bars</div><div style="padding: 3px;">${barHtml}</div>
		<div>Beats per bar</div><div style="padding: 3px;">${beatsHtml}</div>
	</div>`;

	
	// Build sample pickers
	var samplePickerhtml = '';
	for (var bank = 0; bank < 2; ++bank) {
		samplePickerhtml += `<select id="samplePicker${bank}" class="docNav" style="display: none" onchange="SampleChanged('samplePicker${bank}');">`;
		for (var sample = 0; sample < sampleList[bank].length; ++sample) {
			samplePickerhtml += `<option value="${sample}">${sampleList[bank][sample].toLowerCase()}</option>`;
		}
		samplePickerhtml += `</select>`;
	}

	// Note editing options (volume and note variation)
	html += 
	`<div style="display: grid; grid-template-columns: 100px 200px 100px 150px; padding: 10px;">
		<div style="padding: 3px;">Volume</div>
		<div style="padding: 10px;">
			<input id="noteLevel" onchange="DrumLevelChanged();" value="100" min="0" max="127" type="range" class="topcoat-range">
		</div>
		<div style="padding: 3px;">Variation</div>
		<div>
			${samplePickerhtml}
		</div>
	</div>`;


	for (var bar = 0; bar < sequenceSettings.bars; ++bar) {
		html += '<div style="display: grid; grid-template-columns: 100px 160px 160px 160px 160px; padding: 5px; border: 1px solid rgba(0, 0, 0, 0.8);">';
		for (let v in voiceEnum) {
			html += `<div class="grid-container3">${v}</div><div class="grid-container3">`;
			for (var beat = 0; beat < sequenceSettings.beatsPerBar; beat++) {
				var barBreak = sequenceSettings.beatsPerBar / 4;
				if (beat % barBreak == 0 && beat > 0) {
					html += '</div><div class="grid-container3">';
				}
				html += `<button id="${bar + v + beat}" class="topcoat-button" onclick="DrumClicked(${bar}, \'${v + beat}\');" onfocusin="BeatGotFocus('${bar + v + beat}');" style="background-color: rgb(74, 77, 78);">&nbsp;</button> `;
			}
			html += "</div>";
		}
		html += '</div>';
	}
	document.getElementById("drumEditor").innerHTML = html;

	// Update the button bar to show current sequence
	ClearButtonBar();
	document.getElementById(`btnSeq${sequenceSettings.seq}`).style.backgroundColor = "#737373";
}


function RefreshSequence(seq)
{
	requestNo = 0;			// request bar 0
	RequestSequence(seq, 0);	
}


function RequestSequence(seq, bar)
{
	// Creates a SysEx request for a voice's configuration
	var message = [0xF0, requestEnum.GetSequence, seq, bar, 0xF7];
    output.send(message);
}


function RefreshConfig()
{
	requestNo = 0;
	RequestConfig(drumSettings[requestNo].id);
}


function RequestConfig(voice)
{
	// Creates a SysEx request for a voice's configuration
	var message = [0xF0, requestEnum.GetVoiceConfig, voice, 0xF7];
    output.send(message);
}


function RequestSamples(bank)
{
	// Creates a SysEx request for a voice's configuration
	var message = [0xF0, requestEnum.GetSamples, bank, 0xF7];
    output.send(message);
}


function onMIDIFailure() 
{
    console.log('Could not access your MIDI devices.');
}


function decodeSysEx(midiData, headerLen)
{
	// As upper bit cannot be set in a SysEx byte, data is sent a nibble at a time - reconstruct into byte array
	var decodedSysEx = [];
	for (i = headerLen; i < midiData.length - 1; ++i) {
		if (i % 2 != 0) {
			decodedSysEx[Math.trunc((i - headerLen) / 2)] = midiData[i];
		} else {
			decodedSysEx[Math.trunc((i - headerLen) / 2)] += (midiData[i] << 4);
		}
	}
	return decodedSysEx;
}


//var sampleList = new Array(2).fill([]);
var sampleList = [];

function getMIDIMessage(midiMessage) 
{
	// Receive MIDI message - process SysEx containing encoded configuration data
	console.log(midiMessage);

    if (midiMessage.data[0] == 0xF0) {

		switch (midiMessage.data[1]) {
			case requestEnum.GetSamples:
				PrintMessage(midiMessage.data, true);			// Print contents of payload to console
				var bank = midiMessage.data[2];
				sampleList[bank] = [];
				
				let decoder = new TextDecoder();				// Decoder to convert from utf8 array to string
				let midiData = midiMessage.data.slice(3);		// remove header
				for (var i = 0; i < Math.trunc(midiData.length / 8); ++i) {
					sampleList[bank][i] = decoder.decode(midiData.slice(i * 8, i * 8 + 8));
				}
				if (++requestNo < 2) {
					RequestSamples(requestNo);
				} else {
					requestNo = 0;
					RequestSequence(127, 0);					// 127 = currently active sequence
				}
				break;

			case requestEnum.GetSequence:
				PrintMessage(midiMessage.data, true);			// Print contents of payload to console

				sequenceSettings.seq = midiMessage.data[2];
				sequenceSettings.beatsPerBar = Math.max(midiMessage.data[3], beatOptions[0]);
				sequenceSettings.bars = Math.max(midiMessage.data[4], 1);
				sequenceSettings.bar = midiMessage.data[5];

				// If received the first drum bar build the html accordingly
				if (requestNo == 0) {
					BuildSequenceHtml();
					document.getElementById(`barCount${sequenceSettings.bars}`).checked = true;
					document.getElementById(`beatsPerBar${sequenceSettings.beatsPerBar}`).checked = true;
				}

				var index = 6;
				for (var b = 0; b < maxBeatsPerBar; b++) {
					for (let v in voiceEnum) {
						if (b < sequenceSettings.beatsPerBar) {
							document.getElementById(sequenceSettings.bar + v + b).setAttribute("level", midiMessage.data[index]);
							document.getElementById(sequenceSettings.bar+ v + b).setAttribute("index", midiMessage.data[index + 1]);
							if (midiMessage.data[index] > 0) {
								document.getElementById(sequenceSettings.bar + v + b).style.backgroundColor = "rgb(236, 81, 78)";
							}
						}
						index += 2;
					}
				}
				
				// Request the configuration data for the first drum voice
				if (++requestNo < sequenceSettings.bars) {
					RequestSequence(sequenceSettings.seq, requestNo);
				}
				break;


			case requestEnum.GetVoiceConfig:
				var sysEx = decodeSysEx(midiMessage.data, 3);		// 3 is length of header
				PrintMessage(sysEx, true);			// Print contents of payload to console

				if (requestNo == 0) {
					BuildConfigHtml();
				}

				// locate settings that match the enum passed
				for (var i = 0; i < drumSettings.length; ++i) {
					if (drumSettings[i].id == midiMessage.data[2]) {
						for (var s = 0; s < drumSettings[i].settings.length; s++) {
							// Store the values encoded in the SysEx data into the html fields
							document.getElementById(drumSettings[i].settings[s].value).value = BytesToFloat(sysEx.slice(s * 4, s * 4 + 4));
						}
					}
				}

				// Request the configuration data for the next voice
				if (++requestNo < drumSettings.length) {
					RequestConfig(drumSettings[requestNo].id);
				}
				break;
			
		}
	}
}


function BytesToFloat(buff) 
{
    var fl = new Float32Array(new Uint8Array(buff).buffer)[0];
	return Math.trunc(fl * 10000000) / 10000000;
}


function updateConfig(index)
{
	// Copy the values of the html fields into a float array for serialisation
	var floatArray = new Float32Array(drumSettings[index].settings.length)
	for (var s = 0; s < floatArray.length; s++) {
		floatArray[s] = document.getElementById(drumSettings[index].settings[s].value).value;
	}
    var byteArray = new Uint8Array(floatArray.buffer);   			// use the buffer of Float32Array view

    // Convert to sysex information
    var message = new Uint8Array(4 + (byteArray.length * 2));
    message[0] = 0xF0;
    message[1] = requestEnum.SetVoiceConfig;                        // Set config command
    message[2] = drumSettings[index].id;
    var msgPos = 3;
   
	// Since the upper bit of a sysex byte cannot be set, split each byte into an upper and lower nibble for transmission
	var lowerNibble = true;
    for (i = 0; i < byteArray.length * 2; ++i) {
		if (lowerNibble) {
			message[msgPos++] = byteArray[Math.trunc(i / 2)] & 0xF;
		} else {
			message[msgPos++] = byteArray[Math.trunc(i / 2)] >> 4;
		}
        lowerNibble = !lowerNibble;
    }
    message[msgPos] = 0xF7;

	PrintMessage(message);			// Print contents of payload to console
    output.send(message);
}


// Sends MIDI note to requested channel
function sendNote(noteValue, channel)
{
    if (checkConnection()) {
        output.send([0x90 + parseInt(channel - 1), noteValue, 0x7f]);
        output.send([0x80 + parseInt(channel - 1), noteValue, 0x40], window.performance.now() + 1000.0);
        // note off delay: now + 1000ms
    }
}


// MIDI Note on
function noteOn(noteValue, channel) 
{
    if (checkConnection()) {
        output.send([0x90 + parseInt(channel - 1), noteValue, 0x7f]);
    }
}


// MIDI note off
function noteOff(noteValue, channel) 
{
    if (checkConnection()) {
        output.send([0x80 + parseInt(channel - 1), noteValue, 0x40]);
    }
}


//Test the MIDI connection is active and update UI accordingly
function checkConnection()
{
    var status = document.getElementById("connectionStatus");
    if (!output || output.state == "disconnected") {
        status.innerHTML = "Disconnected";
        status.style.color = "#d6756a";
        return false;
    } else {
        status.innerHTML = "Connected";
        status.style.color = "#7dce73";
        return true;
    }
}


function onMIDISuccess(midiAccess)
{
	// Check which MIDI interface is Punck and if found start requesting configuration from interface
	midi = midiAccess;    // store in the global variable

    console.log(midiAccess);

    for (var input of midiAccess.inputs.values()) {
        console.log(input.name);
        if (input.name == "Mountjoy Punck MIDI") {
            input.onmidimessage = getMIDIMessage;
        }
    }
    for (var out of midiAccess.outputs.values()) {
        if (out.name == "Mountjoy Punck MIDI") {
            output = out;
        }
    }

    // Update UI to show connection status
    if (checkConnection()) {
		requestNo = 0;
		RequestSamples(0);
    }
}