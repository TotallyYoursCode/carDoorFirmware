#include "timer.h"

volatile uint32_t counter;
void timerInit(void)
{
	TIM2_TimeBaseInit(TIM2_PRESCALER_1, TIMER_CLOCK/TIMER_PERIOD_US-1);
	TIM2_Cmd(ENABLE);
	TIM2_ITConfig(TIM2_IT_UPDATE, ENABLE);
	counter = 0;
}

INTERRUPT_HANDLER(TIM2_UPD_OVF_BRK_IRQHandler, 13)
{
	TIM2_ClearITPendingBit(TIM2_IT_UPDATE);
	counter++;
}

uint32_t millis(void)
{
	uint32_t ret;
	disableInterrupts();
	ret = counter;
	enableInterrupts();
	return ret;
}