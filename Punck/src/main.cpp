#include <configManager.h>
#include "initialisation.h"
#include "USB.h"
#include "Filter.h"
#include "VoiceManager.h"
#include "ExtFlash.h"
#include "FatTools.h"


volatile uint32_t SysTickVal;		// 1 ms resolution
extern uint32_t SystemCoreClock;

// Create DMA buffer that need to live in non-cached memory area
volatile uint16_t __attribute__((section (".dma_buffer"))) ADC_array[ADC1_BUFFER_LENGTH + ADC2_BUFFER_LENGTH];

// TODO:
// Sample panning (naming? web interface?)
// USB does not restart when unplugged and re-plugged in
// Green LED too dim, red too bright
// Problem where sampler voice triggered by MIDI occasionally does not play during sequence
// Performance updates
// Add claps voice

USB usb;

extern "C" {
#include "interrupts.h"
}


int main(void) {

	SystemClock_Config();			// Configure the clock and PLL
	SystemCoreClockUpdate();		// Update SystemCoreClock (system clock frequency)
	InitSysTick();

#if (USB_DEBUG)
	uart.Init();						// Used on Nucleo for debugging USB
#endif

	InitPWMTimer();					// PWM Timers used for adjustable LED brightness
	InitRNG();						// Init random number generator
	InitMidiUART();					// UART for receiving serial MIDI
	InitADC();						// ADCs used to monitor potentiometer inputs
	InitDAC();						// Available on debug pins
	InitCache();					// Configure MPU to not cache memory regions where DMA buffers reside
	InitMDMA();						// Initialise MDMA for background QSPI Flash transfers

	extFlash.Init();				// Initialise external QSPI Flash
	InitIO();						// Initialise buttons, switches and Tempo out
	configManager.RestoreConfig();	// Restore configuration settings (voice config, MIDI mapping, drum sequences)

	usb.Init();
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

