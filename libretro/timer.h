#ifndef _WINXL_TIMER_H
#define _WINXL_TIMER_H

extern uint32_t tick;
extern uint32_t timercnt;

void Timer_Init(void);
uint16_t Timer_GetCount(void);

#endif /* _WINXL_TIMER_H */
