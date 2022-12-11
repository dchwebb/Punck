# Punck
![Image](https://raw.githubusercontent.com/dchwebb/Punck/master/Graphics/Punck_Front.jpg "icon")

Overview
--------

- Audio Demo with Kick, snare and hi hats: [Punck.wav](https://raw.githubusercontent.com/dchwebb/Punck/master/Drum_Sequences/Punck.wav)
- Audio Demo with all voices and samples: [Punck2.wav](https://raw.githubusercontent.com/dchwebb/Punck/master/Drum_Sequences/Punck2.wav)

Punck is drum machine designed for use with Eurorack modular synthesizers. Its five primary voices are Kick, Snare, High Hat and two sample playback channels. In addition Toms and Claps voices are available via MIDI.

It features a built-in drum sequencer with five patterns each with up to 4 bars in length and either 16 or 24 beats per bar.

Voices can be triggered via front panel illuminated buttons, gate input, USB MIDI, serial MIDI and the drum sequencer. All trigger modes are available simultaneously.

Samples are stored on 32MB of internal Flash memory. This storage space is made available as a standard USB drive, requiring no external drives or SD cards.

A browser-based editor allows drum sequences to be entered graphically and detailed parameters of internal voices to be edited. A MIDI mapping facility provides access to different articulations of voices and different samples for the sampler tracks.


Audio DSP
---------

### Kick
The kick drum is modelled on a Roland TR-808 kick. This is generated in five phases, the first three providing an initial steep ramp and discontinuity for the 'click', a fast sine wave at 130Hz and then a slowly decelerating sine wave starting at 60Hz. A potentiometer-controlled 2-pole low pass filter softens the attack and a decay control adjusts the rate at which the level is reduced.

### Snare
The snare is formed of 3 pitched sine wave partials representing the body of the drum and a noise component representing the snare. The partials form the first three modes of vibration of the body. The initial frequency is potentiometer-controlled and the next two partials pitched at multiples of 1.588 and 1.833. The noise component is derived from two random number readings (left and right) and has a decay rate separate from the overall volume envelope. A potentiometer-controlled 2-pole low pass filter attenuates the high frequencies.

### Hi hat
The hi hat consists of 6 square waves with varying rates and levels of frequency modulation. In addition a stereo noise component is used at the beginning of the note. Separate 2-pole high and low pass filters use different ramp envelopes to increase the frequency of the HP filter and reduce the frequency of the LP filter as the note sustains. The decay of the noise and FM partials are separately configurable. When a range of MIDI notes is used to control the hi hat each note will result in a successively more 'open' hi hat sound.

### Sampler A and B
Two independent sample playback voices are provided. These play wave files (8, 16, 24 or 32 bit) stored in the internal flash storage. Each sampler can only play a single sample at a time allowing a maximum of two simultaneously playing voices. Each sampler has a speed control allowing a playback range of 0.5 - 1.5 original speed. Base playback speed is normalised to 48kHz from the original sample rate.

The sample's file name determines the bank (A or B) and index of the sample. Optionally adding suffix '.vNN' to the sample name will set the initial volume of the sample where NN is a value from 0 - 200% and 100% is the original sample level. Eg naming a sample 'B3crash.v150.wav' will make it the 3rd sample on bank B and will play at 150% of its original level.

### Toms
The Toms voice has no UI but is accessible from MIDI and the internal sequencer. An initial fast ramp is followed by a decelerating sine wave. Allocating multiple MIDI notes to playback will allow a range of pitches to be mapped over the MIDI notes.

### Claps
The Claps have no UI but are accessible from MIDI and the internal sequencer. Four 'claps' are played at slightly varying intervals with a sustained phase at the end of the fourth. Each clap is formed of band-pass filtered noise with a center frequency of 1250Hz and a Q of 3.

### Reverb
The reverb engine is derived from Geraint Luff's design: [Signalsmith Audio](https://signalsmith-audio.co.uk/writing/2021/lets-write-a-reverb/). This divides the stereo dry audio into 8 channels which then pass through various diffusion stages followed by a feedback mixer. The diffusion stages use short delay lines and a Hadamard mixing matrix to create a short diffused reverb. The feedback mixer uses longer delays to spread the diffusion channels. The 8 reverb channels are then mixed down to stereo and blended with the dry signal. A 2-pole Low pass filter is used at the input to control high end.


Architecture
------------

![Image](https://raw.githubusercontent.com/dchwebb/Punck/master/Graphics/Punck_Back.jpg "icon")

### Microcontroller

An STM32H743 microcontroller is responsible for voice generation, USB and MIDI handling, interfacing with the Flash storage devices via QSPI and true random number generation used to generate white noise for various voices. Code is written in C++ 20 using the STM32CubeIDE Eclipse-based IDE. All hardware interfacing is carried out through custom code (ie no STM HAL libraries).

Two internal ADCs read the various potentiometers using DMA circular mode. Timers in PWM mode are used to variably illuminate the LEDs integrated into the push buttons.

An external crystal clocked at 12MHz is scaled using the internal PLL to a main system clock of 400MHz. USB is clocked from the internal 48 MHz RC oscillator.

The microcontroller operates in device mode simultaneously managing three USB classes:

- Mass Storage Class:
Used to provide access to the internal sample flash memory so the storage can be accessed as a normal USB Flash drive. The storage is formatted using FAT16 file system.

- MIDI/Audio Class: 
Bi-directional MIDI is supported to enable playback of drum voices from a USB host. A sysex implementation allows the Browser editor to exchange configuration and playback information with the module.

- Communication Device Class:
A console application is provided to manage and debug the module. This provides information general maintenance tools, configuration options and tools for managing the internal flash storage (formatting, directory listings, disk analysis etc).

![Image](https://raw.githubusercontent.com/dchwebb/Punck/master/Graphics/serial.png "icon")

### Flash Storage
32MB of internal Flash memory is provided to allow internal sample storage. This consists of two 128Mbit Winbond W25Q128JVS chips connected via QSPI. The MCU operates the flash storage in Dual-flash mode, where 8 bits can be sent/received simultaneously by accessing two Flash memories in parallel. The memory is clocked at ~28MHz.

### Audio DAC
Punck uses a TI PCM5100APW Audio DAC clocked at 48kHz in stereo. The MCU interfaces with the DAC via I2S.

### Audio and Input Conditioning
The stereo audio signal is amplified with a gain of -2.74 using a TL074 op-amp. This also amplifies the tempo clock out signal to around a 5V level. Input clamping is done with MMBT3904 transistors.

### Power Supplies

The Eurorack +/-12V rails have reverse polarity protection and filtering and are then scaled to separate digital and analog 3.3V rails. The 3.3V MCU and DAC digital power supplies are provided by a TI TPS561201 switched-mode synchronous step-down power supply. An LD1117-3.3 linear regulator supplies the analog 3.3V power. A common digital/analog ground plane is used.

- +12V Current Draw: 123mA
- -12V Current Draw: 8mA

Construction
------------

The module is constructed using a sandwich of three PCBs. Schematic and PCB layout carried out in Kicad v6.08.

The component board is a four layer PCB with top mounted components, two inner ground layers and mainly power routing on the bottom layer. 

![Image](https://raw.githubusercontent.com/dchwebb/Punck/master/Graphics/components.png "icon")

The control board is a two layer board on which are mounted the jacks, potentiometers, buttons and USB. 

![Image](https://raw.githubusercontent.com/dchwebb/Punck/master/Graphics/controls.png "icon")

The front panel is a two layer black PCB with exposed coated copper labels and marking.

The prototype was hand-soldered in sections to test each part of the hardware in isolation. This picture shows the power supplies (right), microcontroller (center) and two flash chips (above and to the left of the MCU). The audio DAC and op-amp will be placed on the left hand side:

![Image](https://raw.githubusercontent.com/dchwebb/Punck/master/Graphics/build.jpg "icon")




Web Editor
----------

A Chrome-based web editor allows editing of the 5 internal drum sequences:

![Image](https://raw.githubusercontent.com/dchwebb/Punck/master/Graphics/seqedit.png "icon")

Each sequence can be up to 4 bars long with either 16 or 24 steps per bar. The level of each hit can be set and voice specific settings applied (eg choosing sample, snare sustain, hi-hat open/closed amount, tom pitch). Drum sequences can be exported and imported as json files.
Playback start/stop and sequence selection can be controlled from the editor.

Internal drum voice and reverb settings can also be edited:

![Image](https://raw.githubusercontent.com/dchwebb/Punck/master/Graphics/voiceedit.png "icon")



Prototype
---------

Development was done on an STM Nucleo development board with external surface mount ICs (flash, DAC) soldered to breakout boards for prototyping:

![Image](https://raw.githubusercontent.com/dchwebb/Punck/master/Graphics/Punck_Dev.jpg "icon")


