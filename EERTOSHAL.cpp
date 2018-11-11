#include "EERTOSHAL.h"


void RunRTOS (void)
{
  // RTOS: Инициализация и запуск таймера для таймерной службы
  // Период ~1ms

  #if defined(RT_USE_DEFAULT_TIMER0_ON_COMPARE)
    RT_OCR=TIMERDIVIDER; //число не имеет значения. можно менять, можно ставить шим
    RT_TIMSK |= (1 << RT_OCIE); 
  #else
    //TODO: Реализовать позже
    //Вставить сообщение об ошибке
  #endif
  
  sei();
}

void StopRTOS (void)
{
  // RTOS: Остановка таймерной службы


  #if defined(RT_USE_DEFAULT_TIMER0_ON_COMPARE)
    RT_TIMSK &= !(1 << RT_OCIE); 
  #else
    //TODO: Реализовать позже
    //Вставить сообщение об ошибке
  #endif
  
  sei();
}

