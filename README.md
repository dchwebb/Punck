# Punck
![Image](https://raw.githubusercontent.com/dchwebb/Punck/master/Graphics/Punck_Front.jpg "icon")

Overview
--------

Punck is drum machine designed for use with Eurorack modular synthesizers. Its five primary voices are Kick, Snare, High Hat and two sample playback channels. In addition a Tom voice is available via MIDI.

It features a built-in drum sequencer with five patterns each with up to 4 bars in length and either 16 or 24 beats per bar.

Voices can be triggered via front panel illuminated buttons, gate input, USB MIDI, serial MIDI and the drum sequencer. All trigger modes are available simultaneously.

Samples are stored on 32MB of internal Flash memory. This storage space is made available as a standard USB drive, requiring no external drives or SD cards.

A browser-based editor allows drum sequences to be entered graphically and detailed parameters of internal voices to be edited.


Architecture
------------

![Image](https://raw.githubusercontent.com/dchwebb/Punck/master/Graphics/Punck_Back.jpg "icon")

### Microcontroller

An STM32H743 microcontroller is responsible for voice generation, USB and MIDI handling, interfacing with the Flash storage devices via QSPI and true random number generation used to generate white noise for various voices.

Two internal ADCs read the various potentiometers using DMA circular mode. Timers in PWM mode are used to variably illuminate the LEDs integrated into the push buttons.

An external crystal provides clocking at 12MHz which is scaled using the internal PLL to a main system clock of 400MHz. USB is clocked from the internal 48 MHz RC oscillator.

The microcontroller operates in device mode simultaneously managing three USB classes:

- Mass Storage Class:
Used to provide access to the internal sample flash memory so the storage can be accessed as a normal USB Flash drive.

- MIDI/Audio Class: 
Bi-directional MIDI is supported to enable playback of drum voices from a USB host. A sysex implementation allows the Browser editor to exchange configuration and playback information with the module.

- Communication Device Class
A console application is provided to manage and debug the module. This provides information general maintenance tools, configuration options and tools for managing the internal flash storage (formatting, directory listings, disk analysis etc).

![Image](https://raw.githubusercontent.com/dchwebb/Punck/master/Graphics/serial.png "icon")

### Flash Storage
32MB of internal Flash memory is provided to allow internal sample storage. This consists of two 128Mbit Winbond W25Q128JVS chips connected via QSPI. The MCU operates the flash storage in Dual-flash mode, where 8 bits can be sent/received simultaneously by accessing two Flash memories in parallel. The memory is clocked at ~28MHz.

### Audio DAC
Punck uses a TI PCM5100APW Audio DAC clocked at 48kHz in stereo. The MCU interfaces with the DAC via I2S.

### Audio and Input Conditioning
The stereo audio signal is amplified with a gain of -2.74 using a TL074 op-amp. This also amplifies the tempo clock out signal to around a 5V level. Input clamping is done with MMBT3904 transistors.
### Power Supplies

The 3.3V MCU and DAC digital power supplies are provided by a TI TPS561201 switched-mode synchronous step-down power supply. An LD1117-3.3 linear regulator supplies the analog 3.3V power supplies.

- +12V Current Draw: 123mA
- -12V Current Draw: 8mA

Web Editor
----------

A web editor allows editing of the 5 internal drum sequences:

![Image](https://raw.githubusercontent.com/dchwebb/Punck/master/Graphics/seqedit.png "icon")

Each sequence can be up to 4 bars long with either 16 or 24 steps per bar. The level of each hit can be set and voice specific settings applied (eg choosing sample, snare sustain, hi-hat open/closed amount, tom pitch). Drum sequences can be exported and imported as json files.
Playback start/stop and sequence selection can be controlled from the editor.

Internal drum voice and reverb settings can also be edited:

![Image](https://raw.githubusercontent.com/dchwebb/Punck/master/Graphics/voiceedit.png "icon")



Prototype and Assembly
----------------------

![Image](https://raw.githubusercontent.com/dchwebb/Punck/master/Graphics/Punck_Dev.jpg "icon")
![Image](https://raw.githubusercontent.com/dchwebb/Punck/master/Graphics/build.jpg "icon")

