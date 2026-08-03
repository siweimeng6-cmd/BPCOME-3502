#ifndef __BSP_PWM_H
#define __BSP_PWM_H

#include "bsp_init.h"
#include "stm32f10x.h"

/*
 * 风扇 PWM / TACH 单路直通（对照 Sheet3「定义」）：
 *   PC6 = FAN_PWM  (输入，核心卡下发的风扇PWM命令)
 *   PC7 = FAN1_PWM (输出，实时转发给风扇)
 *   PA9 = FAN1_TACH(输入，风扇实际转速信号)
 *   PA8 = FAN_TACH (输出，实时转发给核心卡)
 * 用 EXTI 双边沿中断做电平实时镜像转发，用 TIM1 自由运行计数做时间戳，
 * 计算频率/占空比仅用于串口上报，不影响转发实时性。
 */

// FAN_PWM 输入 / FAN1_PWM 输出
#define FAN_PWM_IN_PORT               GPIOC
#define FAN_PWM_IN_PIN                GPIO_Pin_6
#define FAN_PWM_IN_PORTSOURCE         GPIO_PortSourceGPIOC
#define FAN_PWM_IN_PINSOURCE          GPIO_PinSource6
#define FAN_PWM_IN_EXTI_LINE          EXTI_Line6

#define FAN1_PWM_OUT_PORT             GPIOC
#define FAN1_PWM_OUT_PIN              GPIO_Pin_7

// FAN1_TACH 输入 / FAN_TACH 输出
#define FAN1_TACH_IN_PORT             GPIOA
#define FAN1_TACH_IN_PIN              GPIO_Pin_9
#define FAN1_TACH_IN_PORTSOURCE       GPIO_PortSourceGPIOA
#define FAN1_TACH_IN_PINSOURCE        GPIO_PinSource9
#define FAN1_TACH_IN_EXTI_LINE        EXTI_Line9

#define FAN_TACH_OUT_PORT             GPIOA
#define FAN_TACH_OUT_PIN              GPIO_Pin_8

// 共用的自由运行时间基准（仅用于测频，不参与转发本身）
#define RELAY_TIMEBASE_TIM            TIM1
#define RELAY_TIMEBASE_TIM_CLK        RCC_APB2Periph_TIM1
#define RELAY_TIMEBASE_PRESCALER      71      // 72MHz / 72 = 1MHz，1us/tick

// 边沿时间戳/频率占空比记录
typedef struct {
    uint16_t last_rising_tick;
    uint16_t high_ticks;
    uint16_t period_ticks;
    uint8_t  primed;       // last_rising_tick 是否已记录过至少一次
    uint8_t  have_period;  // period_ticks 是否已经是有效值
} RELAY_SignalTypeDef;

extern RELAY_SignalTypeDef g_fan_pwm_relay;    // PC6->PC7
extern RELAY_SignalTypeDef g_fan_tach_relay;   // PA9->PA8

void pwm_init(void);
void tach_init(void);
void pwm_func(void);

#endif
