if (navigator.requestMIDIAccess) {
    console.log('This browser supports WebMIDI');
} else {
    console.log('WebMIDI is not supported in this browser.');
}

var midi = null;			// global MIDIAccess object
var output = null;
var requestNo = 0;			// Config is retrieved in chunks - this holds index of current chunk of data being recovered
var barClip = null;			// Clipboard for copying and pasting bars

// enum from c++ code to match voice
var voiceEnum = {
    Kick: 0, Snare: 1, HiHat: 2, Sampler_A: 3, Sampler_B: 4, Toms: 5
};

var requestEnum = {
    StartStop: 0x1A, GetSequence: 0x1B, SetSequence: 0x2B, GetVoiceConfig: 0x1C, SetVoiceConfig: 0x2C, GetSamples: 0x1D, GetStatus: 0x1E, GetADC: 0x1F, SetADC: 0x2F
};


var kickSettings = [
	{name: 'Ramp 1 Inc'},
	{name: 'Ramp 2 Inc'},
	{name: 'Ramp 3 Inc'},
	{name: 'Fast Sine Freq'},
	{name: 'Slow Sine Freq'},
	{name: 'Sine Slow Down Rate'},
];

var tomsSettings = [
	{name: 'Decay Partial 1'},
	{name: 'Decay Partial 2'},
	{name: 'Ramp Inc'},
	{name: 'Init Sine Freq'},
	{name: 'Sine 1 freq scale'},
	{name: 'Sine 2 freq scale'},
	{name: 'Sine 1 level'},
	{name: 'Sine 2 level'},
	{name: 'sineSlowDownRate'},
];

var snareSettings = [
	{name: 'Noise level'},
	{name: 'Noise Decay'},

	{name: 'Base Frequency'},
	{name: 'Base Start Position'},
	{name: 'Partial Decay'},
	
	{name: 'Partial 0 Level'},
	{name: 'Partial 1 Level'},
	{name: 'Partial 2 Level'},
	
	{name: 'Partial 0 Freq Offset'},
	{name: 'Partial 1 Freq Offset'},
	{name: 'Partial 2 Freq Offset'},
];

var hihatSettings = [
	{name: 'Attack'}, 
	{name: 'Decay'},
	
	{name: 'HP Initial Cutoff'},
	{name: 'HP Final Cutoff'},
	{name: 'HP Cutoff  Inc'},
	{name: 'LP Initial Cutoff'},
	{name: 'LP Final Cutoff'},
	{name: 'LP Cutoff  Inc'},

	{name: 'Noise level'},
	{name: 'Noise Decay'},

	{name: 'Partial 0 Level'},
	{name: 'Partial 1 Level'},
	{name: 'Partial 2 Level'},
	{name: 'Partial 3 Level'},
	{name: 'Partial 4 Level'},
	{name: 'Partial 5 Level'},

	{name: 'Partial 0 Frequency'},
	{name: 'Partial 1 Frequency'},
	{name: 'Partial 2 Frequency'},
	{name: 'Partial 3 Frequency'},
	{name: 'Partial 4 Frequency'},
	{name: 'Partial 5 Frequency'},
];


var ADC_Controls = [
	{name: 'Tempo', value: 'ADC_Tempo'},

	{name: 'Kick Level', value: 'ADC_KickLevel'},
	{name: 'Kick Attack', value: 'ADC_KickFilter'},
	{name: 'Kick Decay', value: 'ADC_KickDecay'},

	{name: 'Snare Level', value: 'ADC_SnareLevel'},
	{name: 'Snare Filter', value: 'ADC_SnareFilter'},
	{name: 'Snare Decay', value: 'ADC_SnareDecay'},
	{name: 'Snare Tuning', value: 'ADC_SnareTuning'},
	
	{name: 'HiHat Level', value: 'ADC_HiHatLevel'},
	{name: 'HiHat Decay', value: 'ADC_HiHatDecay'},

	{name: 'Sample A Level', value: 'ADC_SampleALevel'},
	{name: 'Sample A Speed', value: 'ADC_SampleASpeed'},
	{name: 'Sample B Level', value: 'ADC_SampleBLevel'},
	{name: 'Sample B Speed', value: 'ADC_SampleBSpeed'},

	{name: 'Toms Level', value: 'ADC_TomsLevel'},
];

var pickerTypeEnum = {discrete: 0, range: 1};

var variationPicker = [
	{voice: 'Sampler_A', picker: 'samplePicker0', pickerBlock: 'samplePickerBlock0', pickerType: pickerTypeEnum.discrete},
	{voice: 'Sampler_B', picker: 'samplePicker1', pickerBlock: 'samplePickerBlock1', pickerType: pickerTypeEnum.discrete},
	{voice: 'HiHat', picker: 'hihatPicker', pickerBlock: 'hihatPickerBlock', pickerType: pickerTypeEnum.range},
	{voice: 'Toms', picker: 'tomsPicker', pickerBlock: 'tomsPickerBlock', pickerType: pickerTypeEnum.range},
];


var drumSettings = [
	{heading: "Kick Settings", id: voiceEnum.Kick, settings: kickSettings},
	{heading: "Toms Settings", id: voiceEnum.Toms, settings: tomsSettings},
	{heading: "Snare Settings", id: voiceEnum.Snare, settings: snareSettings},
	{heading: "Hihat Settings", id: voiceEnum.HiHat, settings: hihatSettings},
]

// Settings related to drum sequence editing
var sampleList = [];			// Array of sample names
var seqSettings = {seq: 0,	beatsPerBar: 16, bars: 2, bar: 0, playing: false, playingSeq: 0, playingBar: 0,  playingBeat:0};
var selectedBeat = "";
var highlitBeat = "";
var beatOptions = [16, 24];
var maxBeatsPerBar = 24;


function SerialiseSequence()
{
	// Save a sequence to a file
	var fileName = `Sequence${seqSettings.seq + 1}.json`;
	var serialData = {};
	
	var i = 0;
	serialData[i++] = seqSettings;
	for (bar = 0; bar < seqSettings.bars; ++bar) {
		for (var b = 0; b < maxBeatsPerBar; b++) {
			for (let v in voiceEnum) {
				var beatElement = document.getElementById(bar + v + b);
				if (beatElement != null) {
					level = parseInt(beatElement.getAttribute("level"));
					index = parseInt(beatElement.getAttribute("index"));
					serialData[i++] = {'pos': bar + v + b, 'level': level, 'index': index}; 
				}
			}
		}
	}

	var a = document.createElement("a");
	var json = JSON.stringify(serialData),
		blob = new Blob([json], {type: "octet/stream"}),
		url = window.URL.createObjectURL(blob);
	a.href = url;
	a.download = fileName;
	a.click();
	window.URL.revokeObjectURL(url);
}


function CopyBar(bar)
{
	// Store bar in 'clipboard'
	barClip = {};
	
	var i = 0;
	for (var b = 0; b < maxBeatsPerBar; b++) {
		for (let v in voiceEnum) {
			var beatElement = document.getElementById(bar + v + b);
			if (beatElement != null) {
				level = parseInt(beatElement.getAttribute("level"));
				index = parseInt(beatElement.getAttribute("index"));
				barClip[i++] = {'v': v, 'b': b, 'level': level, 'index': index}; 
			}
		}
	}
}


function PasteBar(bar)
{
	// Store bar in 'clipboard'
	if (barClip == null) {
		return;
	}
	
	var i = 0;
	var beat = barClip[i];
	while (beat != null) {
		var beatElement = document.getElementById(bar + barClip[i].v + barClip[i].b);
		if (beatElement != null) {
			beatElement.setAttribute("level", barClip[i].level);
			beatElement.setAttribute("index", barClip[i].index);
			BeatColour(beatElement.id);
		}
		beat = barClip[++i];
	}

	// transmit sequence to module
	for (var b = 0; b < seqSettings.bars; ++b) {
		UpdateDrum(b);
	}
}


function DeserialiseDrum(data)
{
	var serialData = JSON.parse(data); 	// main object
	seqSettings.bars = serialData[0].bars;
	seqSettings.beatsPerBar = serialData[0].beatsPerBar;
	BuildSequenceHtml();
	var i = 1;
	var item = serialData[1];
	while (item != null) {
		var beat = document.getElementById(item.pos);
		beat.setAttribute("level", item.level);
		beat.setAttribute("index", item.index);
		BeatColour(beat.id);
		item = serialData[++i];
	}

	// transmit sequence to module
	for (var b = 0; b < seqSettings.bars; ++b) {
		UpdateDrum(b);
	}
}


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
	message[2] = seqSettings.seq;
	message[3] = 0xF7;

	PrintMessage(message);			// Print contents of payload to console
	output.send(message);
	StatusTimer(100);
}


function UpdateDrum(bar, seqStructureChanged)
{
	// Convert drum sequence to sysex information
	var ve = Object.keys(voiceEnum).length;
	var seqLen = maxBeatsPerBar * ve * 2;

	var message = new Uint8Array(7 + seqLen);
	message[0] = 0xF0;
	message[1] = requestEnum.SetSequence;
	message[2] = seqSettings.seq;
	message[3] = seqSettings.beatsPerBar;
	message[4] = seqSettings.bars;
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
		RefreshSequence(seqSettings.seq);
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
	html += `&nbsp;&nbsp;<button id="btnEditConfig" class="topcoat-button-bar__button--large" onclick="RefreshConfig();" style="background-color: rgb(74, 77, 78)">Drum Settings</button>
			 &nbsp;&nbsp;<button id="btnEditADC" class="topcoat-button-bar__button--large" onclick="RefreshADC();" style="background-color: rgb(74, 77, 78)">ADC Editor</button>
		</div>`;
	document.getElementById("buttonBar").innerHTML = html;
	ClearButtonBar();

	// set up sequence importer
	var importer = document.getElementById("seqImporter");
	importer.addEventListener("change", function() {
		if (this.files && this.files[0]) {
			var myFile = this.files[0];
			var reader = new FileReader();
			reader.addEventListener('load', function(e) {
				DeserialiseDrum(e.target.result)
			});
			reader.readAsBinaryString(myFile);
		}   
	  });
	
    navigator.requestMIDIAccess({ sysex: true }).then(onMIDISuccess, onMIDIFailure);
}


function ClearButtonBar()
{
	// Reset all button bar colours so selected button/tab can be lightened
	for (var s = 0; s < 6; ++s) {
		document.getElementById(`btnSeq${s}`).style.backgroundColor = "rgb(74, 77, 78)";
	}
	document.getElementById(`btnEditConfig`).style.backgroundColor = "rgb(74, 77, 78)";
	document.getElementById(`btnEditADC`).style.backgroundColor = "rgb(74, 77, 78)";
}


function BuildConfigHtml()
{
	// Build html lists of settings for each drum voice
	var html = '<div style="display: grid; grid-template-columns: 250px 100px; padding: 10px; border: 1px solid rgba(0, 0, 0, 0.3);">';
	for (var i = 0; i < drumSettings.length; ++i) {
		html += `<div id="settingsBlock${i}" style="grid-column: 1 / 3;  padding: 10px" onclick="ShowSetting();">${drumSettings[i].heading}</div>`;
		for (var s = 0; s < drumSettings[i].settings.length; s++) {
	 		html += `<div class="grid-container3">${drumSettings[i].settings[s].name}</div>` +
					`<div class="grid-container3"><input type="text" id="drumSettings${i}${s}" onchange="updateConfig(${i});"></div>`;		//id="${drumSettings[i].settings[s].value}"
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
		document.getElementById(selectedBeat).style.border = "1px solid #333434";
	}

	// Set border of selected note to white
	selectedBeat = button;
	document.getElementById(button).style.border = "1px solid #ffffff";
	if (document.getElementById(button).getAttribute("level") > 0) {
		document.getElementById("noteLevel").value = document.getElementById(button).getAttribute("level");
	}

	// FIXME - automatically get variations for all voice types
	for (i = 0; i < variationPicker.length; i++) {
		var picker = document.getElementById(variationPicker[i].picker);
		var pickerBlock = document.getElementById(variationPicker[i].pickerBlock);
		var currentPicker = button.includes(variationPicker[i].voice);
		pickerBlock.style.display = currentPicker ? "block" : "none";

		// If the drum beat is active update the picker
		if (currentPicker && document.getElementById(button).getAttribute("level") > 0) {
			picker.value = document.getElementById(button).getAttribute("index");
		}
	}
	
}


function BlendColour(lowColour, highColour, range)
{
	return [
		range * (highColour[0] - lowColour[0]) +  lowColour[0],
		range * (highColour[1] - lowColour[1]) +  lowColour[1],
		range * (highColour[2] - lowColour[2]) +  lowColour[2]
	];
}


function BeatColour(button)
{
	// Highlight beats according to variation (colour) and volume (brightness)

	// Get Picker type - discrete pickers (eg sampler) use one colour per index, range pickers use a spread of colours for index from 0 - 127
	for (const picker of variationPicker) {
		if (button.includes(picker.voice)) {
			var pt = picker.pickerType;
		}
	}

	var beatColour = [
		{high: [250, 81, 78], low: [100, 77, 78]},			// red
		{high: [14, 200, 20], low: [74, 100, 78]},			// green
		{high: [61, 94, 237], low: [74, 77, 100]},			// blue
		{high: [234, 206, 49], low: [100, 100, 78]},		// yellow
		{high: [158, 100, 205], low: [80, 77, 100]},		// purple
	];

	beat = document.getElementById(button);
	noteLevel = parseInt(beat.getAttribute("level"));
	noteIndex = parseInt(beat.getAttribute("index"));

	var colourRange;
	if (pt == pickerTypeEnum.discrete) {
		var colourIndex = noteIndex < beatColour.length ? noteIndex : 0;
		colourRange = beatColour[colourIndex];
	} else 	if (pt == pickerTypeEnum.range) {
		// Create blend of colours storing the bright and dark values so next caluculation can set level
		hColour = BlendColour(beatColour[0].high, beatColour[2].high, noteIndex / 127);
		lColour = BlendColour(beatColour[0].low, beatColour[2].low, noteIndex / 127);
		colourRange = {high: hColour, low: lColour};
	} else {
		colourRange = beatColour[0];
	}

	if (noteLevel > 0) {
		var newColour = BlendColour(colourRange.low, colourRange.high, (noteLevel / 127));
		beat.style.backgroundColor = `rgb(${newColour[0]}, ${newColour[1]}, ${newColour[2]})`;
	} else {
		beat.style.backgroundColor = "rgb(74, 77, 78)";
	}
}


function DrumClicked(bar, note)
{
	var changed = false;
	noteLevel = parseInt(document.getElementById(bar + note).getAttribute("level"));
	noteIndex = parseInt(document.getElementById(bar + note).getAttribute("index"));
	if (noteLevel > 0) {
		// If mode is click to add/delete set level to 0
		if (document.querySelector('input[name="editMode"]:checked').id == "editModeDelete") {
			document.getElementById(bar + note).setAttribute("level", "0");
			changed = true;
		}
	} else {
		document.getElementById(bar + note).setAttribute("level", document.getElementById("noteLevel").value);
		document.getElementById(bar + note).setAttribute("index", ActivePickerValue(bar + note));
		changed = true;
	}

	BeatColour(bar + note);
	BeatGotFocus(bar + note);					// Clear border of selected note and set new border
	if (changed) {
		UpdateDrum(bar);
	}
}


function DrumLevelChanged()
{
	if (selectedBeat != "") {
		document.getElementById(selectedBeat).setAttribute("level", document.getElementById("noteLevel").value);
		UpdateDrum(parseInt(selectedBeat.substring(0, 1)));
		BeatColour(selectedBeat);
	}
}


function ActivePickerValue(button)
{
	// Return the variation picker associated with the button
	for (i = 0; i < variationPicker.length; i++) {
		var picker = document.getElementById(variationPicker[i].picker);
		if (button.includes(variationPicker[i].voice)) {
			return picker.value;
		}
	}
	return 0;
}


function PickerChanged(picker)
{
	if (selectedBeat != "") {
		document.getElementById(selectedBeat).setAttribute("index", document.getElementById(picker).value);
		UpdateDrum(parseInt(selectedBeat.substring(0, 1)));
		BeatColour(selectedBeat);
	}
}


function SequenceChanged()
{
	// check if editing sequence settings
	var newBars = document.querySelector('input[name="barCount"]:checked').id.substring(8);
	var newBeats = document.querySelector('input[name="beatsPerBar"]:checked').id.substring(11);
	if (seqSettings.bars != newBars || seqSettings.beatsPerBar != newBeats) {
		seqStructureChanged = true;
		seqSettings.bars = newBars;
		seqSettings.beatsPerBar = newBeats;
	}
	UpdateDrum(0, true);			// inform update method that a structural change has occurred
}


function ADCChanged()
{
	var adcSettings = new Uint16Array(ADC_Controls.length);

	for (var i = 0; i < ADC_Controls.length; ++i) {
		adcSettings[i] = document.getElementById(ADC_Controls[i].value).value;
	}
	var byteArray = new Uint8Array(adcSettings.buffer);

    // Convert to sysex information
    var message = new Uint8Array(3 + (byteArray.length * 2));
    message[0] = 0xF0;
    message[1] = requestEnum.SetADC;
    var msgPos = 2;
   
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


function BuildADCHtml(settings)
{
	var adcSettings = new Uint16Array(new Uint8Array(settings).buffer);

	// Build html lists of settings for each drum voice
	var html = '<div style="display: grid; grid-template-columns: 150px 300px; padding: 10px; border: 1px solid rgba(0, 0, 0, 0.3);">';
	for (var i = 0; i < ADC_Controls.length; ++i) {
		html += `<div class="grid-container3">${ADC_Controls[i].name}</div>` +
				`<div style="padding-top: 12px;">
					<input id="${ADC_Controls[i].value}" onchange="ADCChanged();" value="${adcSettings[i]}" min="0" max="65535" type="range" class="topcoat-range">	
				 </div>`;
	}

	html += '</div>'

	document.getElementById("drumEditor").innerHTML = html;

	ClearButtonBar();
	document.getElementById(`btnEditADC`).style.backgroundColor = "#737373";

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
		samplePickerhtml += `<div id="samplePickerBlock${bank}" style="display: none;">
								<label style="padding: 5px;">Sample</label>
								<select id="samplePicker${bank}" class="docNav" onchange="PickerChanged('samplePicker${bank}');">`;
		for (var sample = 0; sample < sampleList[bank].length; ++sample) {
			samplePickerhtml += `<option value="${sample}">${sampleList[bank][sample].toLowerCase()}</option>`;
		}
		samplePickerhtml += `</select></div>`;
	}

	// Add option pickers for hihat and toms
	samplePickerhtml += `<div id="hihatPickerBlock" style="display: none;">
							<label style="padding: 5px;">Open  </label>   
							<input id="hihatPicker" onchange="PickerChanged('hihatPicker');" value="0" min="0" max="127" type="range" class="topcoat-range" style="vertical-align: middle;">
						 </div>
						 <div id="tomsPickerBlock" style="display: none;">
							<label style="padding: 5px;">Pitch  </label>   
							<input id="tomsPicker" onchange="PickerChanged('tomsPicker');" value="0" min="0" max="127" type="range" class="topcoat-range" style="vertical-align: middle;">
						 </div>`;

	// Note editing options (volume and note variation)
	html += 
	`<div style="display: grid; grid-template-columns: 100px 200px 300px; padding: 10px;">
		<div style="padding: 3px;">Volume</div>
		<div style="padding: 10px;">
			<input id="noteLevel" onchange="DrumLevelChanged();" value="100" min="0" max="127" type="range" class="topcoat-range">
		</div>
		
		<div style="display: flex; gap: 10px">
			${samplePickerhtml}
		</div>
	</div>`;


	for (var bar = 0; bar < seqSettings.bars; ++bar) {
		html += `<div id="bar${bar}" style="display: grid; width: fit-content; margin-bottom: 6px; grid-template-columns: 100px 160px 160px 160px 160px 180px; padding: 5px; border: 1px solid rgba(0, 0, 0, 0.3);">`;
		for (let v in voiceEnum) {
			html += `<div class="grid-container3">${v}</div><div class="grid-container3">`;
			for (var beat = 0; beat < seqSettings.beatsPerBar; beat++) {
				var barBreak = seqSettings.beatsPerBar / 4;
				if (beat % barBreak == 0 && beat > 0) {
					html += '</div><div class="grid-container3">';
				}
				html += `<button id="${bar + v + beat}" class="topcoat-button" onclick="DrumClicked(${bar}, \'${v + beat}\');" onfocusin="BeatGotFocus('${bar + v + beat}');" style="background-color: rgb(74, 77, 78);">&nbsp;</button> `;
			}
			html += "</div>";

			// add copy/paste option to first row
			if (voiceEnum[v] == 0) {
				html += `<div class="grid-container3">
							<button class="topcoat-button-bar__button--large" onclick="CopyBar(${bar});">Copy</button>
							<button class="topcoat-button-bar__button--large" onclick="PasteBar(${bar});">Paste</button>
						 </div>`;
			} else {
				html += `<div></div>`;
			}
		}
		html += '</div>';
	}
	document.getElementById("drumEditor").innerHTML = html;

	// Update the button bar to show current sequence
	ClearButtonBar();
	document.getElementById(`btnSeq${seqSettings.seq}`).style.backgroundColor = "#737373";
	document.getElementById(`barCount${seqSettings.bars}`).checked = true;
	document.getElementById(`beatsPerBar${seqSettings.beatsPerBar}`).checked = true;
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
	PrintMessage(message);			// Print contents of payload to console
    output.send(message);
}


function RequestStatus()
{
	// Creates a SysEx request for current playing position
	var message = [0xF0, requestEnum.GetStatus, 0, 0xF7];
	PrintMessage(message);			// Print contents of payload to console
    output.send(message);
}


function RefreshConfig()
{
	requestNo = 0;
	RequestConfig(drumSettings[requestNo].id);
}


function RefreshADC()
{
	var message = [0xF0, requestEnum.GetADC, 0, 0xF7];
	PrintMessage(message);			// Print contents of payload to console
    output.send(message);
}


function RequestConfig(voice)
{
	// Creates a SysEx request for a voice's configuration
	var message = [0xF0, requestEnum.GetVoiceConfig, voice, 0xF7];
	PrintMessage(message);			// Print contents of payload to console
    output.send(message);
}


function RequestSamples(bank)
{
	// Creates a SysEx request for a voice's configuration
	var message = [0xF0, requestEnum.GetSamples, bank, 0xF7];
	PrintMessage(message);			// Print contents of payload to console
    output.send(message);
}


function onMIDIFailure() 
{
    console.log('Could not access your MIDI devices.');
}


function DecodeSysEx(data, headerLen)
{
	// As upper bit cannot be set in a SysEx byte, data is sent a nibble at a time - reconstruct into byte array
	var decodedSysEx = [];
	var midiData = data.slice(headerLen);
	for (i = 0; i < midiData.length - 1; ++i) {
		if (i % 2 == 0) {
			decodedSysEx[Math.trunc((i) / 2)] = midiData[i];
		} else {
			decodedSysEx[Math.trunc((i) / 2)] += (midiData[i] << 4);
		}
	}
	// for (i = headerLen; i < midiData.length - 1; ++i) {
	// 	if (i % 2 != 0) {
	// 		decodedSysEx[Math.trunc((i - headerLen) / 2)] = midiData[i];
	// 	} else {
	// 		decodedSysEx[Math.trunc((i - headerLen) / 2)] += (midiData[i] << 4);
	// 	}
	// }
	return decodedSysEx;
}


function DisplayPosition()
{
var highlightOn = document.getElementById("autoUpdate").checked && seqSettings.playing;

	// Clear border of previously selected note
	if (highlitBeat != "" && highlitBeat != selectedBeat && document.getElementById(highlitBeat) != null) {
		document.getElementById(highlitBeat).style.border = "1px solid #333434";		// FIXME - change to got focus colour if necessary
	}

	// Highlight currently playing beat on kick channel
	highlitBeat = seqSettings.playingBar + "Kick" + seqSettings.playingBeat;
	if (highlightOn && document.getElementById(highlitBeat) != null) {
		document.getElementById(highlitBeat).style.border = "1px solid #bbbbbb";		// FIXME - change to got focus colour if necessary
	}	

	// Updates the currently playing bar - FIXME should update sequence display if changed on module
	for (var i = 0; i < 4; ++i) {
		var barGrid = document.getElementById("bar" + i);
		if (barGrid != null) {
			if (highlightOn && seqSettings.playingSeq == seqSettings.seq && i == seqSettings.playingBar) {
				barGrid.style.border = "1px solid rgba(255, 0, 0, 0.8)";
			} else {
				barGrid.style.border = "1px solid rgba(0, 0, 0, 0.3)";
			}
		}
	}
	StatusTimer();
}


function StatusTimer(wait)
{
	// Request playing position update periodically
	if (wait == null) {
		wait = seqSettings.playing ? 300 : 1500;
	}
	if (document.getElementById("autoUpdate").checked) {
		setTimeout(function(){ RequestStatus(); }, wait);
	} else {
		DisplayPosition();			// Clear highlight bar/beat
	}
}


function getMIDIMessage(midiMessage) 
{
	// Receive MIDI message - process SysEx containing encoded configuration data
	console.log(midiMessage);

    if (midiMessage.data[0] == 0xF0) {

		switch (midiMessage.data[1]) {
			case requestEnum.GetADC:
				var sysEx = DecodeSysEx(midiMessage.data, 2);	// 2 is length of header
				PrintMessage(sysEx, true);						// Print contents of payload to console
				BuildADCHtml(sysEx);
				break;

			case requestEnum.GetStatus:
				PrintMessage(midiMessage.data, true);			// Print contents of payload to console
				seqSettings.playing = midiMessage.data[2];
				seqSettings.playingSeq = midiMessage.data[3];
				seqSettings.playingBar = midiMessage.data[4];
				seqSettings.playingBeat = midiMessage.data[5];
				DisplayPosition();
				break;

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

				seqSettings.seq = midiMessage.data[2];
				seqSettings.beatsPerBar = Math.max(midiMessage.data[3], beatOptions[0]);
				seqSettings.bars = Math.max(midiMessage.data[4], 1);
				seqSettings.bar = midiMessage.data[5];

				// If received the first drum bar build the html accordingly
				if (requestNo == 0) {
					BuildSequenceHtml();
				}

				var index = 6;
				for (var b = 0; b < maxBeatsPerBar; b++) {
					for (let v in voiceEnum) {
						if (b < seqSettings.beatsPerBar) {
							document.getElementById(seqSettings.bar + v + b).setAttribute("level", midiMessage.data[index]);
							document.getElementById(seqSettings.bar+ v + b).setAttribute("index", midiMessage.data[index + 1]);
							if (midiMessage.data[index] > 0) {
								BeatColour(seqSettings.bar + v + b);
							}						
						}
						index += 2;
					}
				}
				
				// Request the configuration data for the first drum voice
				if (++requestNo < seqSettings.bars) {
					RequestSequence(seqSettings.seq, requestNo);
				}
				break;


			case requestEnum.GetVoiceConfig:
				var sysEx = DecodeSysEx(midiMessage.data, 3);		// 3 is length of header
				PrintMessage(sysEx, true);							// Print contents of payload to console

				if (requestNo == 0) {
					BuildConfigHtml();
				}

				// locate settings that match the enum passed
				for (var i = 0; i < drumSettings.length; ++i) {
					if (drumSettings[i].id == midiMessage.data[2]) {
						for (var s = 0; s < drumSettings[i].settings.length; s++) {
							// Store the values encoded in the SysEx data into the html fields
							document.getElementById(`drumSettings${i}${s}`).value = BytesToFloat(sysEx.slice(s * 4, s * 4 + 4));
							//document.getElementById(drumSettings[i].settings[s].value).value = BytesToFloat(sysEx.slice(s * 4, s * 4 + 4));
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
	return Math.trunc(fl * 1000000) / 1000000;
}


function BytesToUInt16(buff) 
{
    return new Uint16Array(new Uint8Array(buff).buffer)[0];
}

function updateConfig(index)
{
	// Copy the values of the html fields into a float array for serialisation
	var floatArray = new Float32Array(drumSettings[index].settings.length)
	for (var s = 0; s < floatArray.length; s++) {
		floatArray[s] = document.getElementById(`drumSettings${index}${s}`).value;
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