if (navigator.requestMIDIAccess) {
    console.log('This browser supports WebMIDI');
} else {
    console.log('WebMIDI is not supported in this browser.');
}

var midi = null;
// global MIDIAccess object
var output = null;
var requestNo = 1;
// stores number of control awaiting configuration information

var voiceEnum = {
    kick: 0, snare: 1, hatClosed: 2, hatOpen: 3, tomHigh: 4, tomMedium: 5, tomLow: 6, samplerA: 7, samplerB: 8
};

var controlEnum = {
    gate: 1,
    cv: 2
};
var recCount = 0;

window.onload = afterLoad;
function afterLoad() {
    // sysex not currently working so no need request permission: requestMIDIAccess({ sysex: true })
    navigator.requestMIDIAccess({ sysex: true }).then(onMIDISuccess, onMIDIFailure);
}


// Check which MIDI interface is Punck and if found start requesting configuration from interface
function onMIDISuccess(midiAccess) {
    midi = midiAccess;
    // store in the global variable

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
        RequestConfig(voiceEnum.snare);
    }
}

function onMIDIFailure() {
    console.log('Could not access your MIDI devices.');
}


//Receive MIDI message - mainly used to process configuration information returned from module
function getMIDIMessage(midiMessage) {
    console.log(midiMessage);

    if (midiMessage.data[0] == 0xF0) {
        // As upper bit cannot be set in a sysEx byte, data is sent a nibble at a time - reconstruct into byte array
        var result = [];
        for (i = 1; i < midiMessage.data.length - 1; ++i) {
            if (i % 2 != 0) {
                result[Math.trunc((i - 1) / 2)] = midiMessage.data[i];
            } else {
                result[Math.trunc((i - 1) / 2)] += (midiMessage.data[i] << 4);
            }
        };
        
    	var response = document.getElementById("testResponse");    
        var stringData = "";
        for (i = 0; i < (midiMessage.data.length / 2) - 1; ++i) {
            stringData += result[i].toString(16) + " ";
        }
        
        response.innerHTML = "Received: " + stringData;

		if (result[0] == 0x1C) {
			sysEx = result.slice(2);
			if (result[1] == voiceEnum.hatClosed) {
				document.getElementById("carrierFreq").value = BytesToFloat(sysEx.slice(0, 4));
	            document.getElementById("modulatorFreq").value = BytesToFloat(sysEx.slice(4, 8));
				document.getElementById("modulatorDuty").value = BytesToFloat(sysEx.slice(8, 12));
				document.getElementById("modulatorHighMult").value = BytesToFloat(sysEx.slice(12, 16));
				document.getElementById("modulatorLowMult").value = BytesToFloat(sysEx.slice(16, 20));
				document.getElementById("decay").value = BytesToFloat(sysEx.slice(20, 24));
			} else if (result[1] == voiceEnum.snare) {
		        document.getElementById("baseFreq").value = BytesToFloat(sysEx.slice(0, 4));
				document.getElementById("partial0Level").value = BytesToFloat(sysEx.slice(4, 8));
		        document.getElementById("partial1Level").value = BytesToFloat(sysEx.slice(8, 12));
				document.getElementById("partial2Level").value = BytesToFloat(sysEx.slice(12, 16));
			}
			if (requestNo < 2) {
	            requestNo++;
	            RequestConfig(requestNo);
	        }
		}
	}
}


function updateRange()
{
	document.getElementById("partialDecay").value = document.getElementById("partialDecayR").value;
}


function RequestConfig(voice)
{
	var message = [0xF0, 0x1C, voice, 0xF7];
    output.send(message);
}


function serialiseConfig(voice)
{

    if (voice == voiceEnum.snare) {
		return new Float32Array([document.getElementById("baseFreq").value,
	              document.getElementById("partial0Level").value,
				  document.getElementById("partial1Level").value, 
				  document.getElementById("partial2Level").value
				 ]);
	} else if (voice == voiceEnum.hatClosed) {
		return new Float32Array([document.getElementById("carrierFreq").value,
				  document.getElementById("modulatorFreq").value,
				  document.getElementById("modulatorDuty").value,
				  document.getElementById("modulatorHighMult").value,
				  document.getElementById("modulatorLowMult").value,
				  document.getElementById("decay").value
				 ]);
	}
}


function updateConfig(voice)
{
	var farr = serialiseConfig(voice);
    var barr = new Uint8Array(farr.buffer);   // use the buffer of Float32Array view

    // Convert to sysex information
    var message = new Uint8Array(4 + (barr.length * 2));
    message[0] = 0xF0;
    message[1] = 0x2C;
    message[2] = voice;
    var msgPos = 3;
    var lowerNibble = true;
    for (i = 0; i < barr.length * 2; ++i) {
		if (lowerNibble) {
			message[msgPos++] = barr[Math.trunc(i / 2)] & 0xF;
		} else {
			message[msgPos++] = barr[Math.trunc(i / 2)] >> 4;
		}
        lowerNibble = !lowerNibble;
    }
    message[msgPos] = 0xF7;
    
    output.send(message);
}


function BytesToFloat(buff) {
    var val = new Float32Array(new Uint8Array(buff).buffer)[0];
    console.log(val);
    return val;
}


// Sends MIDI note to requested channel
function sendNote(noteValue, channel) {
    if (checkConnection()) {
        output.send([0x90 + parseInt(channel - 1), noteValue, 0x7f]);
        output.send([0x80 + parseInt(channel - 1), noteValue, 0x40], window.performance.now() + 1000.0);
        // note off delay: now + 1000ms
    }
}


// MIDI Note on
function noteOn(noteValue, channel) {
    if (checkConnection()) {
        output.send([0x90 + parseInt(channel - 1), noteValue, 0x7f]);
    }
}


// MIDI note off
function noteOff(noteValue, channel) {
    if (checkConnection()) {
        output.send([0x80 + parseInt(channel - 1), noteValue, 0x40]);
    }
}


//Test the MIDI connection is active and update UI accordingly
function checkConnection() {
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




function testSysex() {
    var message = [0xF0, 0x1C, voiceEnum.snare, 0xF7];
    output.send(message);
}