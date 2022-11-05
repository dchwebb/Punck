#include "initialisation.h"
#include "USB.h"
#include "Filter.h"
#include "VoiceManager.h"
#include "config.h"
#include "ExtFlash.h"
#include "FatTools.h"


volatile uint32_t SysTickVal;		// 1 ms resolution
extern uint32_t SystemCoreClock;

// Store buffers that need to live in special memory areas
volatile uint16_t __attribute__((section (".dma_buffer"))) ADC_array[ADC2_BUFFER_LENGTH];

// TODO:
// Sample panning (naming? web interface?)


USB usb;

extern "C" {
#include "interrupts.h"
}

int main(void) {

	SystemClock_Config();			// Configure the clock and PLL
	SystemCoreClockUpdate();		// Update SystemCoreClock (system clock frequency)
	InitSysTick();

#if (USB_DEBUG)
	InitUART();						// Used on Nucleo for debugging USB
#endif

	InitPWMTimer();
	InitRNG();						// Init random number generator
//	InitMidiUART();
	InitADC();
	InitDAC();
	InitCache();					// Configure MPU to not cache memory regions where DMA buffers reside
	InitMDMA();						// Initialise MDMA for background QSPI Flash transfers

	extFlash.Init();				// Initialise external QSPI Flash
	InitIO();						// Initialise switches and LEDs
//	config.RestoreConfig();			// Restore configuration settings (ADC offsets etc)

//	usb.Init();
//	InitI2S();						// Initialise I2S which will start main sample interrupts

	while (1) {
	//	usb.cdc.ProcessCommand();	// Check for incoming USB serial commands
		fatTools.CheckCache();		// Check if any outstanding cache changes need to be written to Flash
	//	voiceManager.IdleTasks();	// Check if filter coefficients need to be updated

#if (USB_DEBUG)
		if ((GPIOC->IDR & GPIO_IDR_ID13) == GPIO_IDR_ID13) {
			usb.OutputDebug();
		}
#endif

	}
}

