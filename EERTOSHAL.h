#ifndef EERTOSHAL_H
#define EERTOSHAL_H

#include <Arduino.h>
#include "HAL.h"



//RTOS DEFINES
#if !defined(RTOS_DEFINES)
  #define RTOS_TIMER_ISR TIMER0_COMPA_vect
  #define RT_OCR OCR0A
  #define RT_TIMSK TIMSK0
  #define RT_OCIE OCIE0A
  #define RT_USE_DEFAULT_TIMER0_ON_COMPARE
#endif

//RTOS Config
#define TaskQueueSize   20
#define MainTimerQueueSize  15

//extern void RunRTOS (void);


void RunRTOS (void);
void StopRTOS (void);



#endif
