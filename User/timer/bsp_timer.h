#ifndef __BSP_TIMER_H
#define	__BSP_TIMER_H

#include "bsp_init.h"

#define            BASIC_TIMER6                   TIM6
#define            BASIC_TIMER6_APBxClock_FUN     RCC_APB1PeriphClockCmd
#define            BASIC_TIMER6_CLK               RCC_APB1Periph_TIM6
#define            BASIC_TIMER6_Period            (1000-1)
#define            BASIC_TIMER6_Prescaler         71
#define            BASIC_TIMER6_IRQ               TIM6_IRQn
#define            BASIC_TIMER6_IRQHandler        TIM6_IRQHandler


#define            BASIC_TIMER7                   TIM7
#define            BASIC_TIMER7_APBxClock_FUN     RCC_APB1PeriphClockCmd
#define            BASIC_TIMER7_CLK               RCC_APB1Periph_TIM7
#define            BASIC_TIMER7_Period            1000-1
#define            BASIC_TIMER7_Prescaler         71
#define            BASIC_TIMER7_IRQ               TIM7_IRQn
#define            BASIC_TIMER7_IRQHandler        TIM7_IRQHandler


/**************************º¯ÊýÉùÃ÷********************************/

void BASIC_TIM_Mode_Config(void);
void line0_shake(void);
void line1_shake(void);
void line9_shake(void);

#endif 
