#include "stm32h743xx.h"
#include "initialisation.h"
#include "extFlash.h"
#include "GpioPin.h"
#include <cstring>

// Clock overview:
// [Main clock (Nucleo): 8MHz (HSE) / 2 (M) * 200 (N) / 2 (P) = 400MHz]
// Main clock (v1): 12MHz (HSE) / 3 (M) * 200 (N) / 2 (P) = 400MHz
// I2S: Peripheral Clock (AKA per_ck) set to HSI = 64MHz
// ADC: Peripheral Clock (AKA per_ck) set to HSI = 64MHz

#if CPUCLOCK == 400
// 8MHz (HSE) / 2 (M) * 200 (N) / 2 (P) = 400MHz
// 12MHz (HSE) / 3 (M) * 200 (N) / 2 (P) = 400MHz
#define PLL_M1 3
#define PLL_N1 200
#define PLL_P1 1			// 0000001: pll1_p_ck = vco1_ck / 2
#define PLL_Q1 4			// This is used for I2S clock: 8MHz (HSE) / 2 (M) * 200 (N) / 4 (Q) = 200MHz
#define PLL_R1 2

#else
// 8MHz (HSE) / 2 (M) * 240 (N) / 2 (P) = 480MHz
#define PLL_M1 2
#define PLL_N1 240
#define PLL_P1 1			// 0000001: pll1_p_ck = vco1_ck / 2
#define PLL_Q1 4			// This is used for I2S clock: 8MHz (HSE) / 2 (M) * 240 (N) / 4 (Q) = 240MHz
#define PLL_R1 2
#endif


void InitClocks()
{
	RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;			// Enable System configuration controller clock

	// Voltage scaling - see Datasheet page 113. VOS1 > 300MHz; VOS2 > 200MHz; VOS3 < 200MHz
	PWR->CR3 &= ~PWR_CR3_SCUEN;						// Supply configuration update enable - this must be deactivated or VOS ready does not come on
	PWR->D3CR |= PWR_D3CR_VOS;						// Configure voltage scaling level 1 before engaging overdrive (0b11 = VOS1)
	while ((PWR->CSR1 & PWR_CSR1_ACTVOSRDY) == 0);	// Check Voltage ready 1= Ready, voltage level at or above VOS selected level

#if CPUCLOCK > 400
	// Activate the LDO regulator overdrive mode. Must be written only in VOS1 voltage scaling mode.
	// This activates VSO0 mode for use in clock speeds from 400MHz to 480MHz
	SYSCFG->PWRCR |= SYSCFG_PWRCR_ODEN;
	while ((SYSCFG->PWRCR & SYSCFG_PWRCR_ODEN) == 0);
#endif

	RCC->CR |= RCC_CR_HSEON;						// Turn on external oscillator
	while ((RCC->CR & RCC_CR_HSERDY) == 0);			// Wait till HSE is ready

	// Clock source to High speed external and main (M) dividers
	RCC->PLLCKSELR = RCC_PLLCKSELR_PLLSRC_HSE |
	                 PLL_M1 << RCC_PLLCKSELR_DIVM1_Pos;

	// PLL 1 dividers
	RCC->PLL1DIVR = (PLL_N1 - 1) << RCC_PLL1DIVR_N1_Pos |
			        PLL_P1 << RCC_PLL1DIVR_P1_Pos |
			        (PLL_Q1 - 1) << RCC_PLL1DIVR_Q1_Pos |
			        (PLL_R1 - 1) << RCC_PLL1DIVR_R1_Pos;

	RCC->PLLCFGR |= RCC_PLLCFGR_PLL1RGE_2;			// 10: The PLL1 input (ref1_ck) clock range frequency is between 4 and 8 MHz (Will be 4MHz for 8MHz clock divided by 2)
	RCC->PLLCFGR &= ~RCC_PLLCFGR_PLL1VCOSEL;		// 0: Wide VCO range:192 to 836 MHz (default after reset)	1: Medium VCO range:150 to 420 MHz
	RCC->PLLCFGR |= RCC_PLLCFGR_DIVP1EN | RCC_PLLCFGR_DIVQ1EN;		// Enable divider outputs

	RCC->CR |= RCC_CR_PLL1ON;						// Turn on main PLL
	while ((RCC->CR & RCC_CR_PLL1RDY) == 0);		// Wait till PLL is ready

	// Peripheral scalers
	RCC->D1CFGR |= RCC_D1CFGR_HPRE_3;				// D1 domain AHB prescaler - divide 400MHz by 2 for 200MHz - this is then divided for all APB clocks below
	RCC->D1CFGR |= RCC_D1CFGR_D1PPRE_2; 			// Clock divider for APB3 clocks - set to 4 for 100MHz: 100: hclk / 2
	RCC->D2CFGR |= RCC_D2CFGR_D2PPRE1_2;			// Clock divider for APB1 clocks - set to 4 for 100MHz: 100: hclk / 2
	RCC->D2CFGR |= RCC_D2CFGR_D2PPRE2_2;			// Clock divider for APB2 clocks - set to 4 for 100MHz: 100: hclk / 2
	RCC->D3CFGR |= RCC_D3CFGR_D3PPRE_2;				// Clock divider for APB4 clocks - set to 4 for 100MHz: 100: hclk / 2


	RCC->CFGR |= RCC_CFGR_SW_PLL1;					// System clock switch: 011: PLL1 selected as system clock
	while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != (RCC_CFGR_SW_PLL1 << RCC_CFGR_SWS_Pos));		// Wait until PLL has been selected as system clock source

	// By default Flash latency is set to 7 wait states - set to 4 for now but may need to increase
	FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_4WS;
	while ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) != FLASH_ACR_LATENCY_4WS);

	SystemCoreClockUpdate();						// Update SystemCoreClock (system clock frequency)
}


void InitHardware()
{
	InitSysTick();
	InitPWMTimer();					// PWM Timers used for adjustable LED brightness
	InitDebugTimer();				// Timer 3 used for performance testing
	InitRNG();						// Init random number generator
	InitMidiUART();					// UART for receiving serial MIDI
	InitADC();						// ADCs used to monitor potentiometer inputs
	//InitDAC();					// Available on debug pins
	InitCache();					// Configure MPU to not cache memory regions where DMA buffers reside
	InitMDMA();						// Initialise MDMA for background QSPI Flash transfers
	InitIO();						// Initialise buttons, switches and Tempo out
}


void InitMDMA()
{
	// Initialises MDMA to background transfers of data from QSPI Flash to RAM
	RCC->AHB3ENR |= RCC_AHB3ENR_MDMAEN;
	MDMA_Channel0->CCR &= ~MDMA_CCR_EN;
	MDMA_Channel0->CCR |= MDMA_CCR_PL_0;			// Priority: 00 = low; 01 = Medium; 10 = High; 11 = Very High

	MDMA_Channel0->CTCR |= MDMA_CTCR_DSIZE_1;		// Destination data size - 00: 8-bit, 01: 16-bit, *10: 32-bit, 11: 64-bit
	MDMA_Channel0->CTCR |= MDMA_CTCR_SSIZE_1;		// Source data size - 00: 8-bit, 01: 16-bit, *10: 32-bit, 11: 64-bit
	MDMA_Channel0->CTCR |= MDMA_CTCR_DINC_1;		// 10: Destination address pointer is incremented after each data transfer
	MDMA_Channel0->CTCR |= MDMA_CTCR_SINC_1;		// 10: Source address pointer is incremented after each data transfer
	MDMA_Channel0->CTCR |= MDMA_CTCR_DINCOS_1;		// Destination increment - 00: 8-bit, 01: 16-bit, *10: 32-bit, 11: 64-bit
	MDMA_Channel0->CTCR |= MDMA_CTCR_SINCOS_1;		// Source increment - 00: 8-bit, 01: 16-bit, *10: 32-bit, 11: 64-bit
	MDMA_Channel0->CTCR |= MDMA_CTCR_BWM;			// Bufferable Write Mode
	MDMA_Channel0->CTCR |= MDMA_CTCR_SWRM;			// Software Request Mode
	MDMA_Channel0->CTCR |= MDMA_CTCR_TRGM;			// 01: Each MDMA request triggers a block transfer

	MDMA_Channel0->CTBR &= ~MDMA_CTBR_SBUS;			// Source: 0* System/AXI bus; 1 AHB bus/TCM
	MDMA_Channel0->CTBR |= MDMA_CTBR_DBUS;			// Destination: 0 System/AXI bus; 1* AHB bus/TCM: used for addresses starting 0x20xxxxxx (DTCMRAM)

	MDMA_Channel0->CCR |= MDMA_CCR_BTIE;			// Enable Block Transfer complete interrupt

	NVIC_SetPriority(MDMA_IRQn, 0x3);				// Lower is higher priority
	NVIC_EnableIRQ(MDMA_IRQn);
}


void MDMATransfer(const uint8_t* srcAddr, const uint8_t* destAddr, const uint32_t bytes)
{
	MDMA_Channel0->CTCR |= ((bytes - 1) << MDMA_CTCR_TLEN_Pos);	// Transfer length in bytes - 1
	MDMA_Channel0->CBNDTR |= (bytes << MDMA_CBNDTR_BNDT_Pos);	// Number of bytes in a block

	MDMA_Channel0->CSAR = (uint32_t)srcAddr;		// Configure the source address
	MDMA_Channel0->CDAR = (uint32_t)destAddr;		// Configure the destination address


	MDMA_Channel0->CCR |= MDMA_CCR_EN;				// Enable DMA
	MDMA_Channel0->CCR |= MDMA_CCR_SWRQ;			// Software Activate the request (fires interrupt when complete)
}


void InitCache()
{
	// Use the Memory Protection Unit (MPU) to set up a region of memory with data caching disabled for use with DMA buffers
	MPU->RNR = 0;									// Memory region number
	MPU->RBAR = reinterpret_cast<uint32_t>(&ADC_array);	// Store the address of the ADC_array into the region base address register

	MPU->RASR = (0b11  << MPU_RASR_AP_Pos)   |		// All access permitted
				(0b001 << MPU_RASR_TEX_Pos)  |		// Type Extension field: See truth table on p228 of Cortex M7 programming manual
				(1     << MPU_RASR_S_Pos)    |		// Shareable: provides data synchronization between bus masters. Eg a processor with a DMA controller
				(0     << MPU_RASR_C_Pos)    |		// Cacheable
				(0     << MPU_RASR_B_Pos)    |		// Bufferable (ignored for non-cacheable configuration)
				(5     << MPU_RASR_SIZE_Pos) |		// Size is log 2(mem size) - 1 ie 2^6 = 64
				(1     << MPU_RASR_ENABLE_Pos);		// Enable MPU region


/*
	MPU->RNR = 1;									// Memory region number
	MPU->RBAR = 0x90000000; 						// Address of the QSPI Flash

	MPU->RASR = (0b11  << MPU_RASR_AP_Pos)   |		// All access permitted
				(0b001 << MPU_RASR_TEX_Pos)  |		// Type Extension field: See truth table on p228 of Cortex M7 programming manual
				(1     << MPU_RASR_S_Pos)    |		// Shareable: provides data synchronization between bus masters. Eg a processor with a DMA controller
				(0     << MPU_RASR_C_Pos)    |		// Cacheable
				(0     << MPU_RASR_B_Pos)    |		// Bufferable (ignored for non-cacheable configuration)
				(17    << MPU_RASR_SIZE_Pos) |		// 256KB - D2 is actually 288K (size is log 2(mem size) - 1 ie 2^18 = 256k)
				(1     << MPU_RASR_ENABLE_Pos);		// Enable MPU region

*/
	MPU->CTRL |= (1 << MPU_CTRL_PRIVDEFENA_Pos) |	// Enable PRIVDEFENA - this allows use of default memory map for memory areas other than those specific regions defined above
				 (1 << MPU_CTRL_ENABLE_Pos);		// Enable the MPU

	// Enable data and instruction caches
	SCB_EnableDCache();
	SCB_EnableICache();
}


void InitSysTick()
{
	SysTick_Config(SystemCoreClock / SYSTICK);		// gives 1ms
	NVIC_SetPriority(SysTick_IRQn, 0);
}


void InitDAC()
{
	// DAC1_OUT2 on PA5
	RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN;			// GPIO port clock
	RCC->APB1LENR |= RCC_APB1LENR_DAC12EN;			// Enable DAC Clock

	DAC1->CR |= DAC_CR_EN1;							// Enable DAC using PA4 (DAC_OUT1)
	DAC1->MCR &= DAC_MCR_MODE1_Msk;					// Mode = 0 means Buffer activated, Connected to external pin

	DAC1->CR |= DAC_CR_EN2;							// Enable DAC using PA5 (DAC_OUT2)
	DAC1->MCR &= DAC_MCR_MODE2_Msk;					// Mode = 0 means Buffer activated, Connected to external pin
}


void InitAdcPins(ADC_TypeDef* ADC_No, const std::initializer_list<uint8_t> channels) {
	uint8_t sequence = 1;

	for (auto channel: channels) {
		// NB reset mode of GPIO pins is 0b11 = analog mode so shouldn't need to change
		ADC_No->PCSEL |= 1 << channel;					// ADC channel preselection register

		// Set conversion sequence to order ADC channels are passed to this function
		if (sequence < 5) {
			ADC_No->SQR1 |= channel << (sequence * 6);
		} else if (sequence < 10) {
			ADC_No->SQR2 |= channel << ((sequence - 5) * 6);
		} else if (sequence < 15)  {
			ADC_No->SQR3 |= channel << ((sequence - 10) * 6);
		} else {
			ADC_No->SQR4 |= channel << ((sequence - 15) * 6);
		}

		// 000: 1.5 ADC clock cycles; 001: 2.5 cycles; 010: 8.5 cycles;	011: 16.5 cycles; 100: 32.5 cycles; 101: 64.5 cycles; 110: 387.5 cycles; 111: 810.5 cycles
		if (channel < 10)
			ADC_No->SMPR1 |= 0b010 << (3 * channel);
		else
			ADC_No->SMPR2 |= 0b010 << (3 * (channel - 10));

		sequence++;
	}
}


// Settings used for both ADC1 and ADC2
void InitADC()
{
	// Configure clocks
	RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN;			// GPIO port clock
	RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
	RCC->AHB4ENR |= RCC_AHB4ENR_GPIOCEN;
	RCC->AHB4ENR |= RCC_AHB4ENR_GPIOFEN;

	RCC->AHB1ENR |= RCC_AHB1ENR_ADC12EN;

	// 00: pll2_p_ck default, 01: pll3_r_ck clock, 10: per_ck clock*
	RCC->D3CCIPR |= RCC_D3CCIPR_ADCSEL_1;			// ADC clock selection: 10: per_ck clock (hse_ck, hsi_ker_ck or csi_ker_ck according to CKPERSEL in RCC->D1CCIPR p.353)
	RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

	ADC12_COMMON->CCR |= ADC_CCR_PRESC_0;			// Set prescaler to ADC clock divided by 2 (if 64MHz = 32MHz)

	InitADC1();
	InitADC2();
}



void InitADC1()
{
	// Initialize ADC peripheral
	DMA1_Stream1->CR &= ~DMA_SxCR_EN;
	DMA1_Stream1->CR |= DMA_SxCR_CIRC;				// Circular mode to keep refilling buffer
	DMA1_Stream1->CR |= DMA_SxCR_MINC;				// Memory in increment mode
	DMA1_Stream1->CR |= DMA_SxCR_PSIZE_0;			// Peripheral size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA1_Stream1->CR |= DMA_SxCR_MSIZE_0;			// Memory size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA1_Stream1->CR |= DMA_SxCR_PL_0;				// Priority: 00 = low; 01 = Medium; 10 = High; 11 = Very High

	DMA1_Stream1->FCR &= ~DMA_SxFCR_FTH;			// Disable FIFO Threshold selection
	DMA1->LIFCR = 0x3F << DMA_LIFCR_CFEIF1_Pos;		// clear all five interrupts for this stream

	DMAMUX1_Channel1->CCR |= 9; 					// DMA request MUX input 9 = adc1_dma (See p.695)
	DMAMUX1_ChannelStatus->CFR |= DMAMUX_CFR_CSOF1; // Channel 1 Clear synchronization overrun event flag

	ADC1->CR &= ~ADC_CR_DEEPPWD;					// Deep powDMA1_Stream2own: 0: ADC not in deep-power down	1: ADC in deep-power-down (default reset state)
	ADC1->CR |= ADC_CR_ADVREGEN;					// Enable ADC internal voltage regulator

	// Wait until voltage regulator settled
	volatile uint32_t wait_loop_index = (SystemCoreClock / (100000UL * 2UL));
	while (wait_loop_index != 0UL) {
		wait_loop_index--;
	}
	while ((ADC1->CR & ADC_CR_ADVREGEN) != ADC_CR_ADVREGEN) {}

	ADC1->CFGR |= ADC_CFGR_CONT;					// 1: Continuous conversion mode for regular conversions
	ADC1->CFGR |= ADC_CFGR_OVRMOD;					// Overrun Mode 1: ADC_DR register is overwritten with the last conversion result when an overrun is detected.
	ADC1->CFGR |= ADC_CFGR_DMNGT;					// Data Management configuration 01: DMA One Shot Mode selected, 11: DMA Circular Mode selected

	// Boost mode 1: Boost mode on. Must be used when ADC clock > 20 MHz.
	ADC1->CR |= ADC_CR_BOOST_1;						// Note this sets reserved bit according to SFR - HAL has notes about silicon revision
	ADC1->SQR1 |= (ADC1_BUFFER_LENGTH - 1);			// For scan mode: set number of channels to be converted

	// Start calibration
	ADC1->CR |= ADC_CR_ADCALLIN;					// Activate linearity calibration (as well as offset calibration)
	ADC1->CR |= ADC_CR_ADCAL;
	while ((ADC1->CR & ADC_CR_ADCAL) == ADC_CR_ADCAL) {};

	/* Configure ADC Channels to be converted:

	PA0 ADC1_INP16   Kick Attack
	PA1 ADC1_INP17   Kick Decay
	PA2 ADC12_INP14  Kick Volume

	PC0 ADC123_INP10 Snare Filter
	PA3 ADC12_INP15  Snare Tuning
	PC4 ADC12_INP4   Snare Volume

	PF11 ADC1_INP2   Sampler A Volume
	PF12 ADC1_INP6   Sampler B Volume
	*/
	InitAdcPins(ADC1, {16, 17, 14, 10, 15, 4, 2, 6});


	// Enable ADC
	ADC1->CR |= ADC_CR_ADEN;
	while ((ADC1->ISR & ADC_ISR_ADRDY) == 0) {}

	DMAMUX1_ChannelStatus->CFR |= DMAMUX_CFR_CSOF1; // Channel 1 Clear synchronization overrun event flag
	DMA1->LIFCR = 0x3F << DMA_LIFCR_CFEIF1_Pos;		// clear all five interrupts for this stream

	DMA1_Stream1->NDTR |= ADC1_BUFFER_LENGTH;		// Number of data items to transfer (ie size of ADC buffer)
	DMA1_Stream1->PAR = (uint32_t)(&(ADC1->DR));	// Configure the peripheral data register address 0x40022040
	DMA1_Stream1->M0AR = (uint32_t)(ADC_array);		// Configure the memory address (note that M1AR is used for double-buffer mode) 0x24000040

	DMA1_Stream1->CR |= DMA_SxCR_EN;				// Enable DMA and wait
	wait_loop_index = (SystemCoreClock / (100000UL * 2UL));
	while (wait_loop_index != 0UL) {
	  wait_loop_index--;
	}

	ADC1->CR |= ADC_CR_ADSTART;						// Start ADC
}


void InitADC2()
{
	// Initialize ADC peripheral
	DMA1_Stream2->CR &= ~DMA_SxCR_EN;
	DMA1_Stream2->CR |= DMA_SxCR_CIRC;				// Circular mode to keep refilling buffer
	DMA1_Stream2->CR |= DMA_SxCR_MINC;				// Memory in increment mode
	DMA1_Stream2->CR |= DMA_SxCR_PSIZE_0;			// Peripheral size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA1_Stream2->CR |= DMA_SxCR_MSIZE_0;			// Memory size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA1_Stream2->CR |= DMA_SxCR_PL_0;				// Priority: 00 = low; 01 = Medium; 10 = High; 11 = Very High

	DMA1_Stream2->FCR &= ~DMA_SxFCR_FTH;			// Disable FIFO Threshold selection
	DMA1->LIFCR = 0x3F << DMA_LIFCR_CFEIF2_Pos;		// clear all five interrupts for this stream

	DMAMUX1_Channel2->CCR |= 10; 					// DMA request MUX input 10 = adc2_dma (See p.695)
	DMAMUX1_ChannelStatus->CFR |= DMAMUX_CFR_CSOF2; // Channel 2 Clear synchronization overrun event flag

	ADC2->CR &= ~ADC_CR_DEEPPWD;					// Deep power down: 0: ADC not in deep-power down	1: ADC in deep-power-down (default reset state)
	ADC2->CR |= ADC_CR_ADVREGEN;					// Enable ADC internal voltage regulator

	// Wait until voltage regulator settled
	volatile uint32_t wait_loop_index = (SystemCoreClock / (100000UL * 2UL));
	while (wait_loop_index != 0UL) {
		wait_loop_index--;
	}
	while ((ADC2->CR & ADC_CR_ADVREGEN) != ADC_CR_ADVREGEN) {}

	ADC2->CFGR |= ADC_CFGR_CONT;					// 1: Continuous conversion mode for regular conversions
	ADC2->CFGR |= ADC_CFGR_OVRMOD;					// Overrun Mode 1: ADC_DR register is overwritten with the last conversion result when an overrun is detected.
	ADC2->CFGR |= ADC_CFGR_DMNGT;					// Data Management configuration 11: DMA Circular Mode selected

	// Boost mode 1: Boost mode on. Must be used when ADC clock > 20 MHz.
	ADC2->CR |= ADC_CR_BOOST_1;						// Note this sets reserved bit according to SFR - HAL has notes about silicon revision
	ADC2->SQR1 |= (ADC2_BUFFER_LENGTH - 1);			// For scan mode: set number of channels to be converted

	// Start calibration
	ADC2->CR |= ADC_CR_ADCALLIN;					// Activate linearity calibration (as well as offset calibration)
	ADC2->CR |= ADC_CR_ADCAL;
	while ((ADC2->CR & ADC_CR_ADCAL) == ADC_CR_ADCAL) {};

	/* Configure ADC Channels to be converted:
	PF13 ADC2_INP2   Tempo Pot

	PA6 ADC12_INP3   Hihat Decay
	PB1 ADC12_INP5   Hihat Volume

	PC1 ADC123_INP11 Sampler A Voice
	PC5 ADC12_INP8   Sampler A Speed

	PA7 ADC12_INP7   Sample B Voice
	PB0 ADC12_INP9   Sampler B Speed
	*/
	InitAdcPins(ADC2, {2, 3, 5, 11, 8, 7, 9});

	// Enable ADC
	ADC2->CR |= ADC_CR_ADEN;
	while ((ADC2->ISR & ADC_ISR_ADRDY) == 0) {}

	DMAMUX1_ChannelStatus->CFR |= DMAMUX_CFR_CSOF2; // Channel 2 Clear synchronization overrun event flag
	DMA1->LIFCR = 0x3F << DMA_LIFCR_CFEIF2_Pos;		// clear all five interrupts for this stream

	DMA1_Stream2->NDTR |= ADC2_BUFFER_LENGTH;		// Number of data items to transfer (ie size of ADC buffer)
	DMA1_Stream2->PAR = reinterpret_cast<uint32_t>(&(ADC2->DR));	// Configure the peripheral data register address 0x40022040
	DMA1_Stream2->M0AR = reinterpret_cast<uint32_t>(&ADC_array[ADC1_BUFFER_LENGTH]);		// Configure the memory address (note that M1AR is used for double-buffer mode) 0x24000040

	DMA1_Stream2->CR |= DMA_SxCR_EN;				// Enable DMA and wait
	wait_loop_index = (SystemCoreClock / (100000UL * 2UL));
	while (wait_loop_index != 0UL) {
		wait_loop_index--;
	}

	ADC2->CR |= ADC_CR_ADSTART;						// Start ADC
}


void InitI2S()
{
	RCC->APB1LENR |= RCC_APB1LENR_SPI2EN;			//	Enable SPI clock

	GpioPin::Init(GPIOB, 12, GpioPin::Type::AlternateFunction, 5);		// PB12 I2S2_WS [alternate function AF5]
	GpioPin::Init(GPIOB, 13, GpioPin::Type::AlternateFunction, 5);		// PB13 I2S2_CK [alternate function AF5]
	GpioPin::Init(GPIOB, 15, GpioPin::Type::AlternateFunction, 5);		// PB15 I2S2_SDO [alternate function AF5]

	// Configure SPI (Shown as SPI2->CGFR in SFR)
	SPI2->I2SCFGR |= SPI_I2SCFGR_I2SMOD;			// I2S Mode
	SPI2->I2SCFGR |= SPI_I2SCFGR_I2SCFG_1;			// I2S configuration mode: 00=Slave transmit; 01=Slave receive; 10=Master transmit*; 11=Master receive

	SPI2->I2SCFGR |= SPI_I2SCFGR_DATLEN_1;			// Data Length 00=16-bit; 01=24-bit; *10=32-bit
	SPI2->I2SCFGR |= SPI_I2SCFGR_CHLEN;				// Channel Length = 32 bits

	SPI2->CFG1 |= SPI_CFG1_UDRCFG_1;				// In the event of underrun resend last transmitted data frame
	SPI2->CFG1 |= SPI_CFG1_FTHLV_1;					// FIFO threshold level. 0001: 2-data

	/* I2S Clock
	000: pll1_q_ck clock selected as SPI/I2S1,2 and 3 kernel clock (default after reset)
	001: pll2_p_ck clock selected as SPI/I2S1,2 and 3 kernel clock
	010: pll3_p_ck clock selected as SPI/I2S1,2 and 3 kernel clock
	011: I2S_CKIN clock selected as SPI/I2S1,2 and 3 kernel clock
	100: per_ck clock selected as SPI/I2S1,2 and 3 kernel clock
	//RCC->D2CCIP1R |= RCC_D2CCIP1R_SPI123SEL;

	I2S Prescaler Clock calculations:
	FS = I2SxCLK / [(32*2)*((2*I2SDIV)+ODD))]

	I2S Clock = 200MHz:			200000000 / (32*2  * ((2 * 32) + 1)) = 48076.92
	I2S Clock = 240MHz:			240000000 / (32*2  * ((2 * 39) + 0)) = 48076.92
	I2S Clock = 300MHz:			280000000 / (32*2  * ((2 * 45) + 1)) = 48076.92
	I2S Clock = 320MHz: 		320000000 / (32*2  * ((2 * 52) + 0)) = 48076.92
	PER_CLK = 64MHz				64000000  / (32*2  * ((2 * 10) + 1)) = 47619.05

	Note timing problems experienced using both pll1_q_ck and pll2_p_ck when FMC controller is using PLL2
	*/
#define I2S_PER_CLK
#ifdef I2S_PLL2P_CLK
	// Use PLL2 clock - configured to 200MHz
	RCC->D2CCIP1R |= RCC_D2CCIP1R_SPI123SEL_0;
	SPI2->I2SCFGR |= (32 << SPI_I2SCFGR_I2SDIV_Pos);// Set I2SDIV to 45 with Odd factor prescaler
	SPI2->I2SCFGR |= SPI_I2SCFGR_ODD;
#endif

#ifdef I2S_PER_CLK
	// Use peripheral clock - configured to internal HSI at 64MHz
	RCC->D2CCIP1R |= RCC_D2CCIP1R_SPI123SEL_2;
	SPI2->I2SCFGR |= (10 << SPI_I2SCFGR_I2SDIV_Pos);// Set I2SDIV to 10 with Odd factor prescaler
	SPI2->I2SCFGR |= SPI_I2SCFGR_ODD;
#endif

#ifdef I2S_DEFAULT
#if CPUCLOCK == 200 | CPUCLOCK == 400
	SPI2->I2SCFGR |= (32 << SPI_I2SCFGR_I2SDIV_Pos);// Set I2SDIV to 32 with Odd factor prescaler
	SPI2->I2SCFGR |= SPI_I2SCFGR_ODD;
#elif CPUCLOCK == 280
	SPI2->I2SCFGR |= (45 << SPI_I2SCFGR_I2SDIV_Pos);// Set I2SDIV to 45 with Odd factor prescaler
	SPI2->I2SCFGR |= SPI_I2SCFGR_ODD;
#elif CPUCLOCK == 320
	SPI2->I2SCFGR |= (52 << SPI_I2SCFGR_I2SDIV_Pos);// Set I2SDIV to 52 with no Odd factor prescaler
#else
	SPI2->I2SCFGR |= (39 << SPI_I2SCFGR_I2SDIV_Pos);// Set I2SDIV to 39 with no Odd factor prescaler
#endif
#endif

	// Enable interrupt when TxFIFO threshold reached, or underrun condition occurs
	SPI2->IER |= (SPI_IER_TXPIE | SPI_IER_UDRIE);

	NVIC_SetPriority(SPI2_IRQn, 0x1);				// Lower is higher priority
	NVIC_EnableIRQ(SPI2_IRQn);

	SPI2->CR1 |= SPI_CR1_SPE;						// Enable I2S
	SPI2->CR1 |= SPI_CR1_CSTART;					// Start I2S
}


void suspendI2S()
{
	SPI2->CR1 |= SPI_CR1_CSUSP;
	while ((SPI2->SR & SPI_SR_SUSP) == 0);
}


void resumeI2S()
{
	// to allow resume from debugging ensure suspended then clear buffer underrun flag
	SPI2->CR1 |= SPI_CR1_CSUSP;
	while ((SPI2->SR & SPI_SR_SUSP) == 0);
	SPI2->IFCR |= SPI_IFCR_UDRC;

	// Clear suspend state and resume
	SPI2->IFCR |= SPI_IFCR_SUSPC;
	while ((SPI2->SR & SPI_SR_SUSP) != 0);
	SPI2->CR1 |= SPI_CR1_CSTART;
}


void InitDebugTimer()
{
	// Configure timer to use in internal debug timing
	RCC->APB1LENR |= RCC_APB1LENR_TIM3EN;
	TIM3->ARR = 65535;
	TIM3->PSC = 1;
	TIM3->CR1 |= TIM_CR1_CEN;
}


void InitIO()
{
	GpioPin::Init(GPIOC, 6,  GpioPin::Type::InputPullup);		// PC6: Seq Select
	GpioPin::Init(GPIOE, 1,  GpioPin::Type::InputPullup);		// PE1: MIDI Learn
	GpioPin::Init(GPIOB, 6,  GpioPin::Type::InputPullup);		// PB6: Kick button
	GpioPin::Init(GPIOB, 5,  GpioPin::Type::InputPullup);		// PB5: Snare button
	GpioPin::Init(GPIOE, 11, GpioPin::Type::InputPullup);		// PE11: HiHat button
	GpioPin::Init(GPIOB, 7,  GpioPin::Type::InputPullup);		// PB7: Sample A button
	GpioPin::Init(GPIOG, 13, GpioPin::Type::InputPullup);		// PG13: Sample B button
	GpioPin::Init(GPIOD, 1,  GpioPin::Type::Input);				// PD1: Kick
	GpioPin::Init(GPIOD, 3,  GpioPin::Type::Input);				// PD3: Snare
	GpioPin::Init(GPIOG, 10, GpioPin::Type::Input);				// PG10: Closed Hihat
	GpioPin::Init(GPIOD, 7,  GpioPin::Type::Input);				// PD7: Open Hihat
	GpioPin::Init(GPIOE, 14, GpioPin::Type::Input);				// PE14: Sampler A
	GpioPin::Init(GPIOE, 15, GpioPin::Type::Input);				// PE15: Sampler B
	GpioPin::Init(GPIOD, 9,  GpioPin::Type::Output);			// PD9: Tempo out
}


void InitQSPI()
{
	RCC->D1CCIPR &= ~RCC_D1CCIPR_QSPISEL;			// 00: hsi_ker_ck clock selected as per_ck cloc
	RCC->AHB3ENR |= RCC_AHB3ENR_QSPIEN;				// Enable QSPI clock

	GpioPin::Init(GPIOB, 2,  GpioPin::Type::AlternateFunction, 9);		// PB2:  CLK Flash 1
	GpioPin::Init(GPIOD, 11, GpioPin::Type::AlternateFunction, 9);		// PD11: IO0 Flash 1
	GpioPin::Init(GPIOD, 12, GpioPin::Type::AlternateFunction, 9);		// PD12: IO1 Flash 1
	GpioPin::Init(GPIOD, 13, GpioPin::Type::AlternateFunction, 9);		// PD13: IO3 Flash 1
	GpioPin::Init(GPIOE, 2,  GpioPin::Type::AlternateFunction, 9);		// PE2:  IO2 Flash 1
	GpioPin::Init(GPIOG, 6,  GpioPin::Type::AlternateFunction, 10);		// PG6:  NCS Flash 1

	GpioPin::Init(GPIOE, 7,  GpioPin::Type::AlternateFunction, 10);		// PE7:  IO0 Flash 2
	GpioPin::Init(GPIOE, 8,  GpioPin::Type::AlternateFunction, 10);		// PE8:  IO1 Flash 2
	GpioPin::Init(GPIOE, 9,  GpioPin::Type::AlternateFunction, 10);		// PE9:  IO2 Flash 2
	GpioPin::Init(GPIOE, 10, GpioPin::Type::AlternateFunction, 10);		// PE10: IO3 Flash 2
	GpioPin::Init(GPIOC, 11, GpioPin::Type::AlternateFunction, 9);		// PC11: NCS Flash 2

	QUADSPI->CR |= 6 << QUADSPI_CR_PRESCALER_Pos;	// Set prescaler to n + 1 => 200MHz / 7 = ~29MHz

	if (dualFlashMode) {
		QUADSPI->DCR |= 24 << QUADSPI_DCR_FSIZE_Pos;// Set bytes in Flash memory to 2^(FSIZE + 1) = 2^25 = 32 Mbytes
		QUADSPI->CR |= QUADSPI_CR_DFM;				// Enable dual flash mode
	} else {
		QUADSPI->DCR |= 23 << QUADSPI_DCR_FSIZE_Pos;// Set bytes in Flash memory to 2^(FSIZE + 1) = 2^24 = 16 Mbytes
		QUADSPI->CR |= QUADSPI_CR_FSEL;				// Flash memory selection: 0: FLASH 1 selected; 1: FLASH 2 selected
	}
}


void InitMidiUART()
{
	RCC->APB1LENR |= RCC_APB1LENR_UART8EN;			// UART8 clock enable
	GpioPin::Init(GPIOE, 0, GpioPin::Type::AlternateFunction, 8);	// PE0 (USART8 RX) AF8

	// By default clock source is muxed to peripheral clock 1 which is system clock / 4 (change clock source in RCC->D2CCIP2R)
	// Calculations depended on oversampling mode set in CR1 OVER8. Default = 0: Oversampling by 16
	const int usartDiv = (SystemCoreClock / 4) / 31250;				//clk / desired_baud

	UART8->BRR |= usartDiv & USART_BRR_DIV_MANTISSA_Msk;
	UART8->CR1 &= ~USART_CR1_M;						// 0: 1 Start bit, 8 Data bits, n Stop bit; 1: 1 Start bit, 9 Data bits, n Stop bit
	UART8->CR1 |= USART_CR1_RE;						// Receive enable
	UART8->CR2 |= USART_CR2_RXINV;					// Invert UART receive to allow use of inverting buffer

	UART8->CR1 |= USART_CR1_RXNEIE;					// Set up interrupts
	NVIC_SetPriority(UART8_IRQn, 3);				// Lower is higher priority
	NVIC_EnableIRQ(UART8_IRQn);

	UART8->CR1 |= USART_CR1_UE;						// UART Enable
}


void InitRNG()
{
	RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;				// Enable clock
	RNG->CR |= RNG_CR_RNGEN;						// Enable random number generator
}


void InitPWMTimer()
{
	// TIM8
	RCC->APB2ENR |= RCC_APB2ENR_TIM8EN;

	GpioPin::Init(GPIOC, 7, GpioPin::Type::AlternateFunction, 3);	// PC7 TIM8 Channel 2
	GpioPin::Init(GPIOC, 8, GpioPin::Type::AlternateFunction, 3);	// PC8 TIM8 Channel 3
	GpioPin::Init(GPIOC, 9, GpioPin::Type::AlternateFunction, 3);	// PC9 TIM8 Channel 4

	// Timing calculations: Clock = 400MHz / (PSC + 1) = 200m counts per second
	// ARR = number of counts per PWM tick = 4096
	// 200m / ARR = 48kHz of PWM square wave with 4096 levels of output
	TIM8->CCMR1 |= TIM_CCMR1_OC2PE;					// Output compare 2 preload enable
	TIM8->CCMR2 |= TIM_CCMR2_OC3PE;					// Output compare 3 preload enable
	TIM8->CCMR2 |= TIM_CCMR2_OC4PE;					// Output compare 4 preload enable

	TIM8->CCMR1 |= (TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2);	// 0110: PWM mode 1 - In upcounting, channel 2 active if TIMx_CNT<TIMx_CCR2
	TIM8->CCMR2 |= (TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2);	// 0110: PWM mode 1 - In upcounting, channel 3 active if TIMx_CNT<TIMx_CCR3
	TIM8->CCMR2 |= (TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2);	// 0110: PWM mode 1 - In upcounting, channel 4 active if TIMx_CNT<TIMx_CCR4

	TIM8->CCR2 = 0;									// Initialise PWM level to zero
	TIM8->CCR3 = 0;
	TIM8->CCR4 = 0;

	TIM8->ARR = 0xFFF;								// Total number of PWM ticks = 4096
	TIM8->PSC = 1;									// Should give ~48kHz
	TIM8->CR1 |= TIM_CR1_ARPE;						// 1: TIMx_ARR register is buffered
	TIM8->CCER |= TIM_CCER_CC2E;					// Capture mode enabled / OC1 signal is output on the corresponding output pin
	TIM8->CCER |= TIM_CCER_CC3E;					// Capture mode enabled / OC1 signal is output on the corresponding output pin
	TIM8->CCER |= TIM_CCER_CC4E;					// Capture mode enabled / OC1 signal is output on the corresponding output pin

	TIM8->BDTR |= TIM_BDTR_MOE;						// Master output enable
	TIM8->EGR |= TIM_EGR_UG;						// 1: Re-initialize the counter and generates an update of the registers
	TIM8->CR1 |= TIM_CR1_CEN;						// Enable counter



	//******************************************************************************
	// TIM4
	RCC->APB1LENR |= RCC_APB1LENR_TIM4EN;

	GpioPin::Init(GPIOD, 14, GpioPin::Type::AlternateFunction, 2);	// PD14 TIM4 Channel 3
	GpioPin::Init(GPIOD, 15, GpioPin::Type::AlternateFunction, 2);	// PD15 TIM4 Channel 4

	// Timing calculations: Clock = 400MHz / (PSC + 1) = 200m counts per second
	// ARR = number of counts per PWM tick = 4096
	// 200m / ARR = 48kHz of PWM square wave with 4096 levels of output
	TIM4->CCMR2 |= TIM_CCMR2_OC3PE;					// Output compare 3 preload enable
	TIM4->CCMR2 |= TIM_CCMR2_OC4PE;					// Output compare 4 preload enable

	TIM4->CCMR2 |= (TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2);	// 0110: PWM mode 1 - In upcounting, channel 3 active if TIMx_CNT<TIMx_CCR3
	TIM4->CCMR2 |= (TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2);	// 0110: PWM mode 1 - In upcounting, channel 4 active if TIMx_CNT<TIMx_CCR4

	TIM4->CCR3 = 0;									// Initialise PWM level to zero
	TIM4->CCR4 = 0;

	TIM4->ARR = 0xFFF;								// Total number of PWM ticks = 4096
	TIM4->PSC = 1;									// Should give ~48kHz
	TIM4->CR1 |= TIM_CR1_ARPE;						// 1: TIMx_ARR register is buffered
	TIM4->CCER |= TIM_CCER_CC3E;					// Capture mode enabled / OC1 signal is output on the corresponding output pin
	TIM4->CCER |= TIM_CCER_CC4E;					// Capture mode enabled / OC1 signal is output on the corresponding output pin

	TIM4->BDTR |= TIM_BDTR_MOE;						// Master output enable
	TIM4->EGR |= TIM_EGR_UG;						// 1: Re-initialize the counter and generates an update of the registers
	TIM4->CR1 |= TIM_CR1_CEN;						// Enable counter

}



void DelayMS(uint32_t ms)
{
	// Crude delay system
	const uint32_t now = SysTickVal;
	while (now + ms > SysTickVal) {};
}


void Reboot()
{
	__disable_irq();
	__DSB();
	NVIC_SystemReset();
}
