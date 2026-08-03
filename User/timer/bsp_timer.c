#include "bsp_init.h"

volatile uint32_t time_line0 = 0; // ms 计时变量 
volatile uint32_t time_line1 = 0; // ms 计时变量 
volatile uint32_t time_line9 = 0; // ms 计时变量 

void BASIC_TIM_Mode_Config(void)
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
		
		// 开启定时器时钟,即内部时钟CK_INT=72M
    BASIC_TIMER6_APBxClock_FUN(BASIC_TIMER6_CLK, ENABLE);
    TIM_TimeBaseStructure.TIM_Period = BASIC_TIMER6_Period;	// 自动重装载寄存器的值，累计TIM_Period+1个频率后产生一个更新或者中断
    TIM_TimeBaseStructure.TIM_Prescaler= BASIC_TIMER6_Prescaler;	  // 时钟预分频数为
    TIM_TimeBaseInit(BASIC_TIMER6, &TIM_TimeBaseStructure);		
    TIM_ClearFlag(BASIC_TIMER6, TIM_FLAG_Update);
    TIM_Cmd(BASIC_TIMER6, ENABLE);	

}








