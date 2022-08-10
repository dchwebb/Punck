void OTG_FS_IRQHandler(void) {
	GPIOD->ODR |= GPIO_ODR_OD2;			// PD2: debug pin

	usb.InterruptHandler();

	GPIOD->ODR &= ~GPIO_ODR_OD2;		// debug

}

//void __attribute__((optimize("O0"))) TinyDelay() {
//	for (int x = 0; x < 2; ++x);
//}


void SPI2_IRQHandler()
{
	// I2S Interrupt

	if ((SPI2->SR & SPI_SR_UDR) == SPI_SR_UDR) {		// Check for Underrun condition
		GPIOG->ODR |= GPIO_ODR_OD11;		// PG11: debug pin green
		SPI2->IFCR |= SPI_IFCR_UDRC;		// Clear underrun condition
		return;
	} else {
		GPIOG->ODR &= ~GPIO_ODR_OD11;
	}

	GPIOC->ODR |= GPIO_ODR_OD11;			// PC11: debug pin blue

	noteHandler.CheckButtons();
	samples.CalcSamples();
	SPI2->TXDR = (uint32_t)(samples.currentSamples[0]);
	SPI2->TXDR = (uint32_t)(samples.currentSamples[1]);

	GPIOC->ODR &= ~GPIO_ODR_OD11;
}


void MDMA_IRQHandler()
{
	// fires when MDMA Flash to memory transfer has completed
	if (MDMA->GISR0 & MDMA_GISR0_GIF0) {
		MDMA_Channel0->CIFCR |= MDMA_CIFCR_CBTIF;		// Clear transfer complete interrupt flag
		usb.msc.DMATransferDone();
	}
}


// System interrupts
void NMI_Handler(void) {}

void HardFault_Handler(void) {
	while (1) {}
}
void MemManage_Handler(void) {
	while (1) {}
}
void BusFault_Handler(void) {
	while (1) {}
}
void UsageFault_Handler(void) {
	while (1) {}
}

void SVC_Handler(void) {}

void DebugMon_Handler(void) {}

void PendSV_Handler(void) {}

void SysTick_Handler(void) {
	++SysTickVal;
}

/*
// USART Decoder
void USART3_IRQHandler() {
	//if ((USART3->ISR & USART_ISR_RXNE_RXFNE) != 0 && !uartCmdRdy) {
	if (!uartCmdRdy) {
		uartCmd[uartCmdPos] = USART3->RDR; 				// accessing RDR automatically resets the receive flag
		if (uartCmd[uartCmdPos] == 10) {
			uartCmdRdy = true;
			uartCmdPos = 0;
		} else {
			uartCmdPos++;
		}
	}
}*/
