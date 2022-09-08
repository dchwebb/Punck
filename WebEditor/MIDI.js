if (navigator.requestMIDIAccess) {
    console.log('This browser supports WebMIDI');
} else {
    console.log('WebMIDI is not supported in this browser.');
}

var midi = null;           // global MIDIAccess object
var output = null;
var requestNo = 0;         // stores number of control awaiting configuration information

// enum from c++ code to match voice
var voiceEnum = {
    kick: 0, snare: 1, hatClosed: 2, hatOpen: 3, tomHigh: 4, tomMedium: 5, tomLow: 6, samplerA: 7, samplerB: 8
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


var drumSettings = [
	{heading: "Kick Settings", id: voiceEnum.kick, settings: kickSettings},
	{heading: "Snare Settings", id: voiceEnum.snare, settings: snareSettings},
	{heading: "Hihat Settings", id: voiceEnum.hatClosed, settings: hihatSettings}
]



window.onload = afterLoad;
function afterLoad() 
{
	// Build html lists of settings for each drum voice
	var html = '';
	for (var i = 0; i < drumSettings.length; ++i) {
		html += '<div style="grid-column: 1 / 3;  padding: 10px">' + drumSettings[i].heading + '</div>';
		for (var s = 0; s < drumSettings[i].settings.length; s++) {
	 		html += '<div class="grid-container3">' + drumSettings[i].settings[s].name + '</div>'+
					'<div class="grid-container3"><input type="text" id="' + drumSettings[i].settings[s].value + '" onchange="updateConfig(' + i + ');"></div>';
		}
	}
	document.getElementById("drumSettings").outerHTML = html;
	
	
    navigator.requestMIDIAccess({ sysex: true }).then(onMIDISuccess, onMIDIFailure);
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
        RequestConfig(drumSettings[0].id);
    }
}


function RequestConfig(voice)
{
	// Creates a SysEx request for a voice's configuration
	var message = [0xF0, 0x1C, voice, 0xF7];
    output.send(message);
}


function onMIDIFailure() 
{
    console.log('Could not access your MIDI devices.');
}


function getMIDIMessage(midiMessage) 
{
	// Receive MIDI message - process SysEx containing encoded configuration data
	console.log(midiMessage);

    if (midiMessage.data[0] == 0xF0) {
        // As upper bit cannot be set in a SysEx byte, data is sent a nibble at a time - reconstruct into byte array
        var decodedSysEx = [];
        for (i = 1; i < midiMessage.data.length - 1; ++i) {
            if (i % 2 != 0) {
                decodedSysEx[Math.trunc((i - 1) / 2)] = midiMessage.data[i];
            } else {
                decodedSysEx[Math.trunc((i - 1) / 2)] += (midiMessage.data[i] << 4);
            }
        };
        
        // Print contents of payload to console
		var stringData = "";
		for (i = 0; i < decodedSysEx.length; ++i) {
            stringData += decodedSysEx[i].toString(16) + " ";
        }
        console.log("Received " + decodedSysEx.length + ": " + stringData);

		if (decodedSysEx[0] == 0x1C) {
			sysEx = decodedSysEx.slice(2);

			// locate settings that match the enum passed
			for (var i = 0; i < drumSettings.length; ++i) {
				if (drumSettings[i].id == decodedSysEx[1]) {
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
    var byteArray = new Uint8Array(floatArray.buffer);   // use the buffer of Float32Array view

    // Convert to sysex information
    var message = new Uint8Array(4 + (byteArray.length * 2));
    message[0] = 0xF0;
    message[1] = 0x2C;                        // Set config command
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

	// Print contents of payload to console
	var stringData = "";
	for (i = 0; i < message.length; ++i) {
		stringData += message[i].toString(16) + " ";
	}
	console.log("Sent " + message.length + ": " + stringData);

    
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


