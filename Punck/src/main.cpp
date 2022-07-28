#include "initialisation.h"
#include "USB.h"
//#include "Filter.h"
#include "SerialHandler.h"
#include "config.h"
#include "ExtFlash.h"
#include "FatTools.h"

volatile uint32_t SysTickVal;		// 1 ms resolution
extern uint32_t SystemCoreClock;

// Store buffers that need to live in special memory areas
volatile uint16_t __attribute__((section (".dma_buffer"))) ADC_array[ADC1_BUFFER_LENGTH + ADC2_BUFFER_LENGTH];

// TODO:
// caching
// Store end address field in sample table (indicating if all memory is contiguous or next cluster must be found)

USB usb;
//Filter filter;
//Config config;

extern "C" {
#include "interrupts.h"
}

int main(void) {

	SystemClock_Config();			// Configure the clock and PLL
	SystemCoreClockUpdate();		// Update SystemCoreClock (system clock frequency)
	InitSysTick();

//	InitADC();
//	InitDAC();						// DAC used to output Wet/Dry mix levels
	InitCache();					// Configure MPU to not cache memory regions where DMA buffers reside
	extFlash.Init();				// Initialise external QSPI Flash
	InitIO();						// Initialise switches and LEDs
//	config.RestoreConfig();			// Restore configuration settings (ADC offsets etc)
//	filter.Init();					// Initialise filter coefficients, windows etc

	usb.Init();
//	InitI2S();						// Initialise I2S which will start main sample interrupts




	while (1) {

		usb.cdc.ProcessCommand();	// Check for incoming USB serial commands

		fatTools.CheckCache();		// Check if any outstanding cache changes need to be written to Flash

//#if (USB_DEBUG)
//		if ((GPIOB->IDR & GPIO_IDR_ID4) == 0 && USBDebug) {
//			USBDebug = false;
//			usb.OutputDebug();
//		}
//#endif

	}
}

