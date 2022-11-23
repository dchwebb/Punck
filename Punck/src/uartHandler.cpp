#include "uartHandler.h"
#include "usb.h"

UART uart;

void UART::Init() {
	// Debug UART pins: PF7 (UART7 TX) and PF6 (UART7 RX)

	RCC->APB1LENR |= RCC_APB1LENR_UART7EN;			// USART7 clock enable
	RCC->AHB4ENR |= RCC_AHB4ENR_GPIOFEN;			// GPIO port F

	GPIOF->MODER &= ~GPIO_MODER_MODE6_0;			// Set alternate function on PF6
	GPIOF->AFR[0] |= 7 << GPIO_AFRL_AFSEL6_Pos;		// Alternate function on PF6 for UART7_RX is AF7
	GPIOF->MODER &= ~GPIO_MODER_MODE7_0;			// Set alternate function on PF7
	GPIOF->AFR[0] |= 7 << GPIO_AFRL_AFSEL7_Pos;		// Alternate function on PF7 for UART7_TX is AF7

	// By default clock source is muxed to peripheral clock 1 which is system clock / 4 (change clock source in RCC->D2CCIP2R)
	// Calculations depended on oversampling mode set in CR1 OVER8. Default = 0: Oversampling by 16
	int USARTDIV = (SystemCoreClock / 4) / 230400;	// clk / desired_baud
	UART7->BRR |= USARTDIV & USART_BRR_DIV_MANTISSA_Msk;
	UART7->CR1 &= ~USART_CR1_M;						// 0: 1 Start bit, 8 Data bits, n Stop bit; 1: 1 Start bit, 9 Data bits, n Stop bit
	UART7->CR1 |= USART_CR1_RE;						// Receive enable
	UART7->CR1 |= USART_CR1_TE;						// Transmitter enable

	// Set up interrupts
	UART7->CR1 |= USART_CR1_RXNEIE;
	NVIC_SetPriority(UART7_IRQn, 6);				// Lower is higher priority
	NVIC_EnableIRQ(UART7_IRQn);

	UART7->CR1 |= USART_CR1_UE;						// UART Enable
}


void UART::SendChar(char c) {
	while ((UART7->ISR & USART_ISR_TXE_TXFNF) == 0);
	UART7->TDR = c;
}

void UART::SendString(const char* s) {
	char c = s[0];
	uint8_t i = 0;
	while (c) {
		while ((UART7->ISR & USART_ISR_TXE_TXFNF) == 0);
		UART7->TDR = c;
		c = s[++i];
	}
}

void UART::SendString(const std::string& s) {
	for (char c : s) {
		while ((UART7->ISR & USART_ISR_TXE_TXFNF) == 0);
		UART7->TDR = c;
	}
}


void UART::ProcessCommand()
{
	if (!commandReady) {
		return;
	}
	std::string_view cmd {command};

	if (cmd.compare("printdebug\n") == 0) {
		usb.OutputDebug();

	} else if (cmd.compare("debugon\n") == 0) {
		extern volatile bool debugStart;
		debugStart = true;
		SendString("Debug activated\r\n");

	} else {
		SendString("Unrecognised command\r\n");
	}

	commandReady = false;
}


extern "C" {

// USART Decoder
void UART7_IRQHandler() {
	if (!uart.commandReady) {
		const uint32_t recData = UART7->RDR;					// Note that 32 bits must be read to clear the receive flag
		uart.command[uart.cmdPos] = (char)recData; 				// accessing RDR automatically resets the receive flag
		if (uart.command[uart.cmdPos] == 10) {
			uart.commandReady = true;
			uart.cmdPos = 0;
		} else {
			uart.cmdPos++;
		}
	}
}
}
