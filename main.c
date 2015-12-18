
#include "stm8s.h"
#include "pins.h"
#include "uartDebug.h"
#include "timer.h"
#include "stdio.h"
#include "bit.h"

volatile uint32_t a;
typedef enum {
	LOCK_NOT_DEFINED = 0,	/* состояние не определено */
	LOCK_OPENED = 1,		/* замок открыт */
	LOCK_PUSHED = 2,		/* замок призакрыт, закрыт не до конца */
	LOCK_CLOSED = 3			/* замок закрыт */
} LockState_t;

typedef enum {
	MOTOR_NOT_DEFINED = 0,	/* состояние не определено */
	MOTOR_CLOSING = 1,		/* движение в направлении закрытия */
	MOTOR_OPENING = 2,		/* движение в направлении открытия */
	MOTOR_PARKING_AFTER_CLOSING = 3,		/* движение к состоянию парковки после закрытия замка */
	MOTOR_PARKING_AFTER_OPENING = 4,		/* движение к состоянию парковки после открытия замка */
	MOTOR_PARKED = 5 		/* запаркован */
} MotorState_t;

typedef enum {
	MODE_WAITING = 0,
	MODE_CLOSING = 1,
	MODE_OPENING = 2
} Mode_t;

LockState_t lockState;
MotorState_t motorState;
Mode_t mode;
uint32_t startTimestamp;

void clockInit(void)
{
	CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV1);
}

void gpioInit(void)
{
	GPIO_Init(Led, GPIO_MODE_OUT_PP_LOW_FAST);		// Led

	GPIO_Init(Button1, GPIO_MODE_IN_PU_NO_IT);	// Open button
	GPIO_Init(Button2, GPIO_MODE_IN_PU_NO_IT);	// Close button
	GPIO_Init(ParkingSwitch, GPIO_MODE_IN_PU_NO_IT);		// RedWire
	GPIO_Init(ClosedSwitch, GPIO_MODE_IN_PU_NO_IT);			// BlueWire
	GPIO_Init(PushedSwitch, GPIO_MODE_IN_PU_NO_IT);			// BlackWire
	GPIO_Init(In4, GPIO_MODE_IN_PU_NO_IT);				// in 4

	GPIO_Init(OutA_Red, GPIO_MODE_OUT_PP_LOW_FAST);		// out A
	GPIO_Init(OutB_Black, GPIO_MODE_OUT_PP_LOW_FAST);		// out B

	GPIO_Init(GPIOD, GPIO_PIN_5, GPIO_MODE_OUT_PP_HIGH_FAST);// UART Tx
	GPIO_Init(GPIOD, GPIO_PIN_6, GPIO_MODE_IN_PU_NO_IT);	// UART Rx
}

#define DEBOUNCE_TIME	(20)
LockState_t getLockState(uint32_t timeNow)
{
	static uint32_t lastTimeUpdate = 0;
	static uint8_t prevState = 0, registeredState = 0;
	uint8_t actState = 0;
	LockState_t lockState;
	if (timeNow - lastTimeUpdate > DEBOUNCE_TIME) {
		lastTimeUpdate = timeNow;

		actState |= GPIO_ReadInputPin(ClosedSwitch) ? 1 : 0;
		actState <<= 1;
		actState |= GPIO_ReadInputPin(PushedSwitch) ? 1 : 0;
		
		if (!(actState^prevState)) {			// если предыдущее и текущее состояния равны (прошел дребезг)
			if (actState^registeredState) {		// если текущее состояние не равно уже зарегистрированному
				registeredState = actState;
			}
		}		
		prevState = actState;
	}
	switch (registeredState) {
		case 0:
			lockState = LOCK_OPENED;
			break;
		case 1:
			lockState = LOCK_PUSHED;
			break;
		case 3:
			lockState = LOCK_CLOSED;
			break;
		case 2:
		default:
			lockState = LOCK_NOT_DEFINED;
			break;
	}	
	return	lockState;
}


void motorClosingDir(void)
{
	GPIO_WriteLow(OutA_Red);
	GPIO_WriteHigh(OutB_Black);
}

void motorOpeningDir(void)
{
	GPIO_WriteLow(OutB_Black);
	GPIO_WriteHigh(OutA_Red);
}

void motorStop(void)
{
	GPIO_WriteLow(OutA_Red);
	GPIO_WriteLow(OutB_Black);
}


#define WAITING_BEFORE_CLOSING 		(1500)
#define CLOSING_TIMEOUT				(1500)
#define PARKING_TIMEOUT				(1500)

#define TIMEOUT_1	(CLOSING_TIMEOUT + WAITING_BEFORE_CLOSING)
#define TIMEOUT_2	(TIMEOUT_1 + PARKING_TIMEOUT)
void closing(uint32_t timeNow)
{
	switch (motorState) {
		case MOTOR_CLOSING: 	// процесс только начался
			if (timeNow - startTimestamp < WAITING_BEFORE_CLOSING)
				return;			// ждем 1.5 сек.
			motorClosingDir();
			if ((lockState == LOCK_CLOSED) || (timeNow - startTimestamp > TIMEOUT_1)) {
				motorStop();
				motorState = MOTOR_PARKING_AFTER_CLOSING;
			}
			break;

		case MOTOR_PARKING_AFTER_CLOSING:
			motorOpeningDir();
			if ((!GPIO_ReadInputPin(ParkingSwitch)) || (timeNow - startTimestamp > TIMEOUT_2)) {
				motorStop();
				motorState = MOTOR_PARKED;
				mode = MODE_WAITING;
			}
			break;

		default:
			break;
	}
}


/** Стадия открытия замка.
	1) Подождать некоторое время после нажатия кнопки
	2) Включить двигатель в сторону открытия замка,
	ждать пока истечет таймаут или концевики замка будут в состоянии открыто.
	3) Выключить двигатель. Подождать пока человек поднимет/откроет дверь (таймаут)
	4) Включить двигатель в сторону парковки (закрытия),
	ждать пока истечет таймаут или сработает концевик парковки
	*/
#define WAITING_BEFORE_OPENING	(1000)
#define OPENING_TIMEOUT			(1500)
#define WAITING_IN_OPEN_STATE 	(1000)

#define TIMEOUT_3	(WAITING_BEFORE_OPENING + OPENING_TIMEOUT)
#define TIMEOUT_4	(TIMEOUT_3 + WAITING_IN_OPEN_STATE)
#define TIMEOUT_5	(TIMEOUT_4 + PARKING_TIMEOUT)

void opening(uint32_t timeNow)
{
	switch (motorState) {
		case MOTOR_OPENING:
			if (timeNow - startTimestamp < WAITING_BEFORE_OPENING)
				return;
			if ((lockState == LOCK_OPENED) || (timeNow - startTimestamp > TIMEOUT_3)) {
				motorStop();
				if (timeNow - startTimestamp > TIMEOUT_4) {
					motorState = MOTOR_PARKING_AFTER_OPENING;
				}
			} else {
				motorOpeningDir();
			}
			break;

		case MOTOR_PARKING_AFTER_OPENING:
			motorClosingDir();
			if ((!GPIO_ReadInputPin(ParkingSwitch)) || (timeNow - startTimestamp > TIMEOUT_4)) {
				motorStop();
				motorState = MOTOR_PARKED;
				mode = MODE_WAITING;
			}
			break;

		default:
			break;
	}
}

void waiting(uint32_t timeNow)
{
	if (lockState == LOCK_OPENED)	// если дверь открыта ничего не делаем
		return;

	if (lockState == LOCK_CLOSED) {			// если дверь закрыта ждем нажатия кнопки
		if (!GPIO_ReadInputPin(Button1)) {
			mode = MODE_OPENING;
			motorState = MOTOR_OPENING;
			startTimestamp = timeNow;
		}
	}

	if (lockState == LOCK_PUSHED) {			// если дверь призакрыта, начинаем процесс закрытия
		mode = MODE_CLOSING;
		motorState = MOTOR_CLOSING;
		startTimestamp = timeNow;
	}
}


int main( void )
{
	clockInit();
	uartDebugInit();
	gpioInit();
	timerInit();
	enableInterrupts();

	lockState = getLockState(millis());
	if (GPIO_ReadInputPin(ParkingSwitch)) {
		motorState = MOTOR_NOT_DEFINED;
	} else {
		motorState = MOTOR_PARKED;
	}

	while(1) {
		uint32_t timeNow = millis();
		lockState = getLockState(timeNow);
		switch (mode) {
			case MODE_WAITING:
				waiting(timeNow);
				break;
			case MODE_CLOSING:
				closing(timeNow);
				break;
			case MODE_OPENING:
				opening(timeNow);
				break;
			default:
				break;
		}
		// ledProcess(timeNow);
	}
}



#ifdef USE_FULL_ASSERT
void assert_failed(u8* file, u32 line){
	printf("Wrong parameters value: file %s on line %d \r\n", (char *)file,(char *)line);
	while (1);
}
#endif