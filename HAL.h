#ifndef HAL_H
#define HAL_H

#include <Arduino.h>
#include "avrlibdefs.h"
#include "avrlibtypes.h"


//RTOS DEFINES
#define RTOS_DEFINES
#define RTOS_TIMER_ISR TIMER0_COMPA_vect
#define RT_OCR OCR0A
#define RT_TIMSK TIMSK0
#define RT_OCIE OCIE0A
#define RT_USE_DEFAULT_TIMER0_ON_COMPARE


#define PRESCALER     64
#define TIMERDIVIDER  (F_CPU/PRESCALER/1000)    // 1 mS

//PORT Defines


//Key Defines

//Key State Defines
#define BTN_NO_PRESSED     0
#define BTN_UP             1
#define BTN_DOWN           2
#define BTN_PRESSED        4
#define BTN_DOUBLE         8

#define UP_BTN 56
#define DOWN_BTN 50
#define LEFT_BTN 52
#define RIGHT_BTN 54
#define OK_BTN 53
#define MENU_BTN 43
#define CANCEL_BTN 48


void InitAll(void);

#endif
