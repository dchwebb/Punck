#pragma once

#include "stm32h7xx.h"
#include "mpu_armv7.h"		// Memory protection unit for selectively disabling cache for DMA transfers
#include <algorithm>
#include <cstdlib>
#include <cmath>

extern volatile uint32_t SysTickVal;

#define TIMINGDEBUG true

#define ADC1_BUFFER_LENGTH 8
#define ADC2_BUFFER_LENGTH 7
#define SYSTICK 1000						// Set in uS so 1000uS = 1ms
#define CPUCLOCK 400

constexpr double pi = 3.14159265358979323846;
constexpr float intToFloatMult = 1.0f / std::pow(2.0f, 31.0f);		// Multiple to convert 32 bit int to -1.0 - 1.0 float
constexpr float floatToIntMult = std::pow(2.0f, 31.0f);				// Multiple to convert -1.0 - 1.0 float to 32 bit int

static constexpr uint32_t systemSampleRate = 48000;
static constexpr float systemMaxFreq = 22000.0f;

extern volatile uint16_t ADC_array[ADC1_BUFFER_LENGTH + ADC2_BUFFER_LENGTH];


// Define ADC array positions of various controls
enum ADC_Controls {
	ADC_KickAttack		= 0,
	ADC_KickDecay		= 1,
	ADC_KickLevel		= 2,

	ADC_SnareFilter 	= 3,
	ADC_SnareTuning		= 4,
	ADC_SnareLevel	 	= 5,

	ADC_SampleALevel	= 6,
	ADC_SampleBLevel	= 7,

	ADC_Tempo 			= 8,

	ADC_HiHatDecay  	= 9,
	ADC_HiHatLevel  	= 10,

	ADC_SampleAVoice	= 11,
	ADC_SampleASpeed	= 12,
	ADC_SampleBVoice	= 13,
	ADC_SampleBSpeed	= 14,

};
enum channel {left = 0, right = 1};

void SystemClock_Config();
void InitCache();
void InitSysTick();
void InitADC();
void InitADC1();
void InitADC2();
void InitDAC();
void InitI2S();
void suspendI2S();
void resumeI2S();
void InitIO();
void InitDebugTimer();
void InitQSPI();
void InitMDMA();
void MDMATransfer(const uint8_t* srcAddr, const uint8_t* destAddr, uint32_t bytes);
void InitMidiUART();
void InitRNG();
void InitPWMTimer();
void DelayMS(uint32_t ms);
void Reboot();
