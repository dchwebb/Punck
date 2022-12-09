#include "configManager.h"
#include "initialisation.h"
#include "USB.h"
#include "VoiceManager.h"
#include "FatTools.h"
#include "Reverb.h"


volatile uint32_t SysTickVal;		// 1 ms resolution
extern uint32_t SystemCoreClock;

// Create DMA buffer that need to live in non-cached memory area
volatile uint16_t __attribute__((section (".dma_buffer"))) ADC_array[ADC1_BUFFER_LENGTH + ADC2_BUFFER_LENGTH];

// Reverb delay lines are large so need to be placed in appropriate memory regions
Reverb __attribute__((section (".ram_d2_data"))) reverb;
float __attribute__((section (".ram_d1_data"))) reverbMixBuffer[94000];

// TODO:
// Sample panning (naming? web interface?)
// Performance updates
// Web editor: finish handling non-float values in config
// Band pass filter to main filter class

USB usb;

extern "C" {
#include "interrupts.h"
}


int main(void) {

	SystemClock_Config();			// Configure the clock and PLL
	SystemCoreClockUpdate();		// Update SystemCoreClock (system clock frequency)
	InitSysTick();

#if (USB_DEBUG)
	uart.Init();					// Used for debugging USB
#endif

	InitPWMTimer();					// PWM Timers used for adjustable LED brightness
	InitDebugTimer();				// Timer 3 used for performance testing
	InitRNG();						// Init random number generator
	InitMidiUART();					// UART for receiving serial MIDI
	InitADC();						// ADCs used to monitor potentiometer inputs
	//InitDAC();					// Available on debug pins
	InitCache();					// Configure MPU to not cache memory regions where DMA buffers reside
	InitMDMA();						// Initialise MDMA for background QSPI Flash transfers

	extFlash.Init();				// Initialise external QSPI Flash
	InitIO();						// Initialise buttons, switches and Tempo out
	configManager.RestoreConfig();	// Restore configuration settings (voice config, MIDI mapping, drum sequences)

	usb.Init(false);				// Pass false to indicate hard reset
	InitI2S();						// Initialise I2S which will start main sample interrupts

	while (1) {
		usb.cdc.ProcessCommand();	// Check for incoming USB serial commands
		fatTools.CheckCache();		// Check if any outstanding cache changes need to be written to Flash
		voiceManager.IdleTasks();	// Check if filter coefficients need to be updated

#if (USB_DEBUG)
		if (uart.commandReady) {
			uart.ProcessCommand();
		}
#endif

	}
}

