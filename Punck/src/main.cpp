#include "initialisation.h"
#include "USB.h"
//#include "Filter.h"
#include "SerialHandler.h"
#include "config.h"
#include "ExtFlash.h"

volatile uint32_t SysTickVal;
extern uint32_t SystemCoreClock;

// Store buffers that need to live in special memory areas
volatile uint16_t __attribute__((section (".dma_buffer"))) ADC_array[ADC1_BUFFER_LENGTH + ADC2_BUFFER_LENGTH];



USB usb;
SerialHandler serial(usb);
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
	InitCache();					// Configure MPU to not cache memory regions where DMA beffers reside
	extFlash.Init();				// Initialise external QSPI Flash
//	InitIO();						// Initialise switches and LEDs
//	config.RestoreConfig();			// Restore configuration settings (ADC offsets etc)
//	filter.Init();					// Initialise filter coefficients, windows etc


//	FRESULT res;                                          // FatFs function common result code
//	uint32_t byteswritten, bytesread;                     // File write/read counts
//	uint8_t wtext[] = "This is STM32 working with FatFs"; // File write buffer
//	uint8_t rtext[100];                                   // File read buffer
//
//	// Link the RAM disk I/O driver
//	if (FATFS_LinkDriver(&ExtFlashDriver, RAMDISKPath) == 0) {
//
//		// Register the file system object to the FatFs module
//		if (f_mount(&RAMDISKFatFs, (char const*)RAMDISKPath, 0) == FR_OK) {
//
//			// Create a FAT file system (format) on the logical drive (Use SFD to optimise space - otherwise partition seems to start at sector 63)
//			if (f_mkfs((TCHAR const*)RAMDISKPath, (FM_ANY | FM_SFD), 0, fsWork, sizeof(fsWork)) == FR_OK) {
//
//				// Create and Open a new text file object with write access
//				if (f_open(&MyFile, "STM32.TXT", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK) {
//
//					// Write data to the text file
//					res = f_write(&MyFile, wtext, sizeof(wtext), (unsigned int*)&byteswritten);
//
//					if ((byteswritten != 0) && (res == FR_OK)) {
//						// Close the open text file
//						f_close(&MyFile);
//
//						// Open the text file object with read access
//						if (f_open(&MyFile, "STM32.TXT", FA_READ) == FR_OK) {
//
//							// Read data from the text file
//							res = f_read(&MyFile, rtext, sizeof(rtext), (unsigned int *)&bytesread);
//
//							if ((bytesread > 0) && (res == FR_OK)) {
//
//								//Close the open text file
//								f_close(&MyFile);
//							}
//						}
//					}
//				}
//
//			}
//
//		}
//
//		DIR dp;						// Pointer to the directory object structure
//		FILINFO fno;				// File information structure
//		res = f_opendir(&dp, "");	// second parm is directory name (root)
//		res = f_readdir(&dp, &fno);
//		uint8_t dummy = 1;
//
//	}



	usb.InitUSB();
//	InitI2S();						// Initialise I2S which will start main sample interrupts

	while (1) {
		serial.Command();			// Check for incoming CDC commands

#if (USB_DEBUG)
		if ((GPIOB->IDR & GPIO_IDR_ID4) == 0 && USBDebug) {
			USBDebug = false;
			usb.OutputDebug();
		}
#endif

	}
}

