#include "uartDebug.h"
#include "stm8s.h"


static volatile uint8_t buf[DEBUG_BUF_LEN], head, tail, cnt;

void uartDebugInit(void)
{
	ITC_SetSoftwarePriority(ITC_IRQ_UART1_TX, ITC_PRIORITYLEVEL_1);	// the lowest priority
	UART1_DeInit();
	UART1_Init((uint32_t)DEBUG_SPEED, UART1_WORDLENGTH_8D, UART1_STOPBITS_1, UART1_PARITY_NO,
				UART1_SYNCMODE_CLOCK_DISABLE, UART1_MODE_TX_ENABLE);	

	/* Enable the UART Transmit interrupt */
	UART1->CR2 |= UART1_CR2_TIEN;

	/* Enable UART */
	UART1->CR1 &= (uint8_t)(~UART1_CR1_UARTD);
	head = tail = cnt = 0;
}
	

int putchar(int c)
{
	UART1->CR2 &= (~UART1_CR2_TIEN);
	if (cnt < DEBUG_BUF_LEN) {
		cnt++;
		buf[tail] = c;
		if (++tail >= DEBUG_BUF_LEN) {
			tail = 0;
		}
	}
	UART1->CR2 |= UART1_CR2_TIEN;
	return c;
}

 INTERRUPT_HANDLER(UART1_TX_IRQHandler, 17)
 {
	 if (cnt > 0) {
		 cnt--;
		 UART1->DR = buf[head];
		 if (++head >= DEBUG_BUF_LEN) {
			 head = 0;
		 }
	 } else {
		 UART1->CR2 &= (~UART1_CR2_TIEN);
	 }
 }