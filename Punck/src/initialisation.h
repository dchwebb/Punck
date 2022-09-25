#pragma once

#include "stm32h7xx.h"
#include "mpu_armv7.h"		// Memory protection unit for selectively disabling cache for DMA transfers
#include <algorithm>
#include <cstdlib>
#include <cmath>

extern volatile uint32_t SysTickVal;

#define ADC1_BUFFER_LENGTH 4				// currently unused
#define ADC2_BUFFER_LENGTH 10
#define WEB_EDITOR_ADC true					// Web editor controls ADC settings
#define SYSTICK 1000						// Set in uS so 1000uS = 1ms
#define ADC_OFFSET_DEFAULT 33800
#define CPUCLOCK 400
constexpr double pi = 3.14159265358979323846;
constexpr float intToFloatMult = 1.0f / std::pow(2.0f, 31.0f);		// Multiple to convert 32 bit int to -1.0 - 1.0 float

static constexpr uint32_t systemSampleRate = 48000;
static constexpr float systemMaxFreq = 22000.0f;

extern volatile uint16_t ADC_array[ADC2_BUFFER_LENGTH];
extern int32_t adcZeroOffset[2];

// Define ADC array positions of various controls
enum ADC_Controls {
	ADC_Tempo 			= 0,
	ADC_KickLevel		= 1,
	ADC_KickAttack		= 2,
	ADC_KickDecay		= 3,
	ADC_SnareFilter 	= 4,
	ADC_SnareDecay  	= 5,
	ADC_SnareTuning		= 6,
	ADC_HiHatDecay  	= 7,
	ADC_SampleASpeed	= 8,
	ADC_SampleBSpeed	= 9,
};
enum channel {left = 0, right = 1};


void SystemClock_Config();
void InitCache();
void InitSysTick();
void uartSendChar(char c);
void uartSendString(const char* s);
void InitADC();
void InitADC1();
void TriggerADC1();
void InitADC2();
void InitDAC();
void InitI2S();
void suspendI2S();
void resumeI2S();
void InitTempoClock();
void InitIO();
void InitDebugTimer();
void CopyToITCMRAM();
void InitQSPI();
void InitMDMA();
void MDMATransfer(const uint8_t* srcAddr, const uint8_t* destAddr, uint32_t bytes);
void InitMidiUART();
void InitRNG();
