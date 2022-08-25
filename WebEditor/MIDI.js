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

//Check which MIDI interface is MIDI Monger and if found start requesting configuration from interface
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
        getOutputConfig(1);
        // get configuration for first port
    }
}

function onMIDIFailure() {
    console.log('Could not access your MIDI devices.');
}

//Receive MIDI message - mainly used to process configuration information returned from module
function getMIDIMessage(midiMessage) {
    console.log(midiMessage);

    if (midiMessage.data[0] == 0xF2) {
        var type = (midiMessage.data[1] & 0xF0) >> 4;
        if (requestNo < 9) {
            document.getElementById("gType" + requestNo).value = type;
            document.getElementById("gChannel" + requestNo).value = (midiMessage.data[1] & 0xF) + 1;
            document.getElementById("gNote" + requestNo).value = midiMessage.data[2];

            updateDisplay(controlEnum.gate, requestNo);
        } else {
            document.getElementById("cType" + (requestNo - 8)).value = type;
            document.getElementById("cChannel" + (requestNo - 8)).value = (midiMessage.data[1] & 0xF) + 1;
            document.getElementById("cController" + (requestNo - 8)).value = midiMessage.data[2];

            updateDisplay(controlEnum.cv, requestNo - 8);
        }

        if (requestNo < 12) {
            requestNo++;
            getOutputConfig(requestNo);

        }
        recCount++;
    } else if (midiMessage.data[0] == 0xF0) {
		  var response = document.getElementById("testResponse");    
        response.innerHTML = "Received: " + midiMessage.data[1];
	 }

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

// Sends an appropriate MIDI signal to test output
function testOutput(outputType, outputNo) {
    if (outputType == controlEnum.cv) {
        var ctlType = parseInt(document.getElementById("cType" + outputNo).value);

        switch (ctlType) {
        case cvEnum.controller:
            var channel = document.getElementById("cChannel" + outputNo).value;
            var controller = document.getElementById("cController" + outputNo).value;
            for (var o = 0; o < 128; o++) {
                output.send([0xB0 + parseInt(channel - 1), controller, o], window.performance.now() + (o * 5));
            }
            break;

        case cvEnum.pitchbend:
            break;

        case cvEnum.aftertouch:
            var channel = document.getElementById("cChannel" + outputNo).value;
            for (var o = 0; o < 128; o++) {
                output.send([0xD0 + parseInt(channel - 1), o], window.performance.now() + (o * 5));
            }
            break;
        }

    } else {
        var ctlType = parseInt(document.getElementById("gType" + outputNo).value);

        switch (ctlType) {
        case gateEnum.specificNote:
            var channel = document.getElementById("gChannel" + outputNo).value;
            var note = document.getElementById("gNote" + outputNo).value;
            output.send([0x90 + parseInt(channel - 1), note, 0x7f]);
            // send each note on command after each interval
            output.send([0x80 + parseInt(channel - 1), note, 0x40], window.performance.now() + 500);
            // send off notes 50ms before next note
            break;
        case gateEnum.clock:
            break;
        }
    }

    // to test MIDI channel send sequence of notes
    if ((outputType == controlEnum.cv && ctlType == cvEnum.channel) || outputType == controlEnum.gate && ctlType == gateEnum.channelNote) {
        var channel = document.getElementById("cChannel" + outputNo).value;
        // send out C1 to C7
        var interval = 500.0;
        // interval between each note
        for (var o = 0; o < 7; o++) {
            output.send([0x90 + parseInt(channel - 1), 24 + (o * 12), 0x7f], window.performance.now() + (o * interval));
            // send each note on command after each interval
            output.send([0x80 + parseInt(channel - 1), 24 + (o * 12), 0x40], window.performance.now() + (o * interval) + (interval - 50));
            // send off notes 50ms before next note
        }
    }
}

// Request configuration for output (uses MIDI song position mechanism)
function getOutputConfig(outputNo) {
    var message = [0xF2, parseInt(outputNo), 0x00];
    output.send(message);
}

// Send out a configuration change to a specific gate
function updateGate(gateNo, cfgType) {
    updateDisplay(controlEnum.gate, gateNo);

    switch (cfgType) {
    case cfgEnum.type:
        var gType = document.getElementById("gType" + gateNo);
        var cfgValue = gType.options[gType.selectedIndex].value;
        break;
    case cfgEnum.specificNote:
        var gNote = document.getElementById("gNote" + gateNo);
        var cfgValue = gNote.options[gNote.selectedIndex].value;
        break;
    case cfgEnum.channel:
        var gChannel = document.getElementById("gChannel" + gateNo);
        var cfgValue = gChannel.options[gChannel.selectedIndex].value;
        break;
    }
    // to send config message use format: ggggtttt vvvvvvvv where g is the gate number and t is the type of setting to pass
    var message = [0xF2, (cfgType << 4) + gateNo, parseInt(cfgValue)];
    // Use MIDI song position to send patch data
    output.send(message);

    // Reload all settings
    requestNo = 1;
    getOutputConfig(1);
}

// Send out a configuration change to a specific cv output
function updateCV(cvNo, cfgType) {
    updateDisplay(controlEnum.cv, cvNo);

    switch (cfgType) {
    case cfgEnum.type:
        var cType = document.getElementById("cType" + cvNo);
        var cfgValue = cType.options[cType.selectedIndex].value;
        break;
    case cfgEnum.channel:
        var cChannel = document.getElementById("cChannel" + cvNo);
        var cfgValue = cChannel.options[cChannel.selectedIndex].value;
        break;
    case cfgEnum.controller:
        var cController = document.getElementById("cController" + cvNo);
        var cfgValue = cController.options[cController.selectedIndex].value;
        break;
    }
    // to send config message use format: ttttoooo vvvvvvvv where o is the output number and t is the type of setting to pass
    var message = [0xF2, (cfgType << 4) + (cvNo + 8), parseInt(cfgValue)];
    // Use MIDI song position to send patch data
    output.send(message);

    // Reload all settings
    requestNo = 1;
    getOutputConfig(1);

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

// Updates configuration blocks to show/hide irrelevant settings
function updateDisplay(controlType, ctlNo) {
    if (controlType == controlEnum.gate) {

        // specific note picker
        var gateShow = document.getElementById("gType" + ctlNo).value == gateEnum.specificNote ? "block" : "none";
        var gateControls = document.getElementById("gateCtl" + ctlNo).getElementsByClassName("gateControl");
        for (var i = 0; i < gateControls.length; i++) {
            gateControls[i].style.display = gateShow;
        }

        // channel picker
        var channelShow = document.getElementById("gType" + ctlNo).value != gateEnum.clock ? "block" : "none";
        var channelControls = document.getElementById("gateCtl" + ctlNo).getElementsByClassName("channelControl");
        for (var i = 0; i < gateControls.length; i++) {
            channelControls[i].style.display = channelShow;
        }
    } else {
        // controller picker
        var controllerShow = document.getElementById("cType" + ctlNo).value == cvEnum.controller ? "block" : "none";
        var controllerControls = document.getElementById("cvCtl" + ctlNo).getElementsByClassName("controllerControl");
        for (var i = 0; i < controllerControls.length; i++) {
            controllerControls[i].style.display = controllerShow;
        }

    }
}

function testSysex() {
    var message = [0xF0, 0x01, 0x02, 0x03, 0x04, 0x0A, 0x0B, 0x0C, 0x0D, 0x11, 0x12, 0x13, 0x14, 0x1A, 0x1B, 0x1C, 0x1D, 0xF7];
    output.send(message);
}