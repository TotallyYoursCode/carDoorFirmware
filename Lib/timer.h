#ifndef __TIMER_H__
#define __TIMER_H__

#include "stm8s.h"

#define TIMER_CLOCK		(16000000UL)
#define TIMER_PERIOD_US	(1000)

void timerInit(void);
uint32_t millis(void);

#endif