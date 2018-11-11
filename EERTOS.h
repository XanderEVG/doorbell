#ifndef EERTOS_H
#define EERTOS_H

#include "EERTOSHAL.h"

typedef void (*TPTR)(void);

extern void InitRTOS(void);
extern void Idle(void);

extern void SetTask(TPTR TS);
extern void SetTimerTask(TPTR TS, u16 NewTime);

void TaskManager(void);

void TimerService(void);

//RTOS Errors Пока не используются.
#define TASK_SET_OK          'A'
#define TASK_QUEUE_OVERFLOW  'B'
#define TIMER_UPDATED        'C'
#define TIMER_SET_OK         'D'
#define TIMER_OVERFLOW       'E'



#endif
