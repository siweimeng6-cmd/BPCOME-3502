#include "timer/bsp_pwm.h"

RELAY_SignalTypeDef g_fan_pwm_relay  = {0, 0, 0, 0, 0};   // PC6(FAN_PWM in) -> PC7(FAN1_PWM out)
RELAY_SignalTypeDef g_fan_tach_relay = {0, 0, 0, 0, 0};   // PA9(FAN1_TACH in) -> PA8(FAN_TACH out)

/*
 * 实时把 in_port/in_pin 的电平镜像到 out_port/out_pin 上（转发），
 * 同时用共用的自由计数时间基准记录周期/高电平时间，仅供 pwm_func() 上报使用。
 */
static void Relay_ProcessEdge(RELAY_SignalTypeDef *sig, GPIO_TypeDef *in_port, uint16_t in_pin,
                               GPIO_TypeDef *out_port, uint16_t out_pin)
{
    uint16_t now = TIM_GetCounter(RELAY_TIMEBASE_TIM);

    if (GPIO_ReadInputDataBit(in_port, in_pin) == Bit_SET)
    {
        // 上升沿
        if (sig->primed)
        {
            sig->period_ticks = (uint16_t)(now - sig->last_rising_tick);
            sig->have_period = 1;
        }
        sig->last_rising_tick = now;
        sig->primed = 1;
        GPIO_SetBits(out_port, out_pin);
    }
    else
    {
        // 下降沿
        if (sig->primed)
        {
            sig->high_ticks = (uint16_t)(now - sig->last_rising_tick);
        }
        GPIO_ResetBits(out_port, out_pin);
    }
}

// PC6/PA9 共用 EXTI9_5 中断向量
void EXTI9_5_IRQHandler(void)
{
    if (EXTI_GetITStatus(FAN_PWM_IN_EXTI_LINE) != RESET)
    {
        Relay_ProcessEdge(&g_fan_pwm_relay, FAN_PWM_IN_PORT, FAN_PWM_IN_PIN,
                           FAN1_PWM_OUT_PORT, FAN1_PWM_OUT_PIN);
        EXTI_ClearITPendingBit(FAN_PWM_IN_EXTI_LINE);
    }

    if (EXTI_GetITStatus(FAN1_TACH_IN_EXTI_LINE) != RESET)
    {
        Relay_ProcessEdge(&g_fan_tach_relay, FAN1_TACH_IN_PORT, FAN1_TACH_IN_PIN,
                           FAN_TACH_OUT_PORT, FAN_TACH_OUT_PIN);
        EXTI_ClearITPendingBit(FAN1_TACH_IN_EXTI_LINE);
    }
}

/***********************************************************************
* @ 函数名  pwm_init
* @ 功能说明  FAN_PWM(PC6) -> FAN1_PWM(PC7) 实时转发初始化，并搭建共用测频时间基准
*********************************************************************/
void pwm_init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    EXTI_InitTypeDef  EXTI_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB2PeriphClockCmd(RELAY_TIMEBASE_TIM_CLK, ENABLE);

    // 共用自由运行计数（1MHz，1us/tick），仅用于测频/占空比上报
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;
    TIM_TimeBaseStructure.TIM_Prescaler = RELAY_TIMEBASE_PRESCALER;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(RELAY_TIMEBASE_TIM, &TIM_TimeBaseStructure);
    TIM_Cmd(RELAY_TIMEBASE_TIM, ENABLE);

    // PC7 - FAN1_PWM 输出，默认低
    GPIO_InitStructure.GPIO_Pin = FAN1_PWM_OUT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(FAN1_PWM_OUT_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(FAN1_PWM_OUT_PORT, FAN1_PWM_OUT_PIN);

    // PC6 - FAN_PWM 输入
    GPIO_InitStructure.GPIO_Pin = FAN_PWM_IN_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(FAN_PWM_IN_PORT, &GPIO_InitStructure);

    GPIO_EXTILineConfig(FAN_PWM_IN_PORTSOURCE, FAN_PWM_IN_PINSOURCE);
    EXTI_InitStructure.EXTI_Line = FAN_PWM_IN_EXTI_LINE;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/***********************************************************************
* @ 函数名  tach_init
* @ 功能说明  FAN1_TACH(PA9) -> FAN_TACH(PA8) 实时转发初始化
*********************************************************************/
void tach_init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    EXTI_InitTypeDef  EXTI_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

    // PA8 - FAN_TACH 输出，默认低
    GPIO_InitStructure.GPIO_Pin = FAN_TACH_OUT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(FAN_TACH_OUT_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(FAN_TACH_OUT_PORT, FAN_TACH_OUT_PIN);

    // PA9 - FAN1_TACH 输入
    GPIO_InitStructure.GPIO_Pin = FAN1_TACH_IN_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(FAN1_TACH_IN_PORT, &GPIO_InitStructure);

    GPIO_EXTILineConfig(FAN1_TACH_IN_PORTSOURCE, FAN1_TACH_IN_PINSOURCE);
    EXTI_InitStructure.EXTI_Line = FAN1_TACH_IN_EXTI_LINE;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}
