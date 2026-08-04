#ifndef __BSP_GPIO_H
#define	__BSP_GPIO_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/**********************************PA***********************************************/
// PA0-WKUP - SELF_RST (自复位，高电平有效，空闲为低)。由UART4 debug口收到"Reset"命令触发，
// 拉高100ms后拉低，见 bsp_gpio.c 的 GPIO_Task() 和 bsp_usart.c 的 UART4_IRQHandler()
#define SELF_RST_GPIO_PORT                      GPIOA
#define SELF_RST_GPIO_PIN                       GPIO_Pin_0

// PA1 - SW_BLACK+ (预留，亮度+按键输入) 暂不使用
#define SW_BLACK_P_GPIO_PORT                    GPIOA
#define SW_BLACK_P_GPIO_PIN                     GPIO_Pin_1

// PA2 - SER_RX1 (预留，与核心卡串口1互连) 暂不使用
#define SER_RX1_GPIO_PORT                       GPIOA
#define SER_RX1_GPIO_PIN                        GPIO_Pin_2

// PA3 - SER_TX1 (预留，与核心卡串口1互连) 暂不使用
#define SER_TX1_GPIO_PORT                       GPIOA
#define SER_TX1_GPIO_PIN                        GPIO_Pin_3

// PA4 - GN32_BL_EN 屏背光使能，高电平有效，参考PB6(P3V3SUS_PG)高后输出高
#define GN32_BL_EN_GPIO_PORT                    GPIOA
#define GN32_BL_EN_GPIO_PIN                     GPIO_Pin_4

// PA5 - PANEL_EN_GD 屏供电使能，高电平有效，参考PB6(P3V3SUS_PG)高后输出高
#define PANEL_EN_GD_GPIO_PORT                   GPIOA
#define PANEL_EN_GD_GPIO_PIN                    GPIO_Pin_5

// PA6 - CB_RESET# 做输入，核心卡复位输出信号
#define CB_RESET_GPIO_PORT                      GPIOA
#define CB_RESET_GPIO_PIN                       GPIO_Pin_6

// PA7 - ADC1 P12V电压采集，系数12
#define ADC1_GPIO_PORT                 GPIOA
#define ADC1_GPIO_PIN                  GPIO_Pin_7

// PA8 - FAN_TACH 输出，转发PA9(FAN1_TACH)测得的风扇转速信号
#define FAN_TACH_GPIO_PORT                      GPIOA
#define FAN_TACH_GPIO_PIN                       GPIO_Pin_8

// PA9 - FAN1_TACH 输入，外部风扇转速信号输入
#define FAN1_TACH_GPIO_PORT                     GPIOA
#define FAN1_TACH_GPIO_PIN                      GPIO_Pin_9

// PA10 - POWER_LED (预留，电源灯状态输出) 暂不使用
#define POWER_LED_GPIO_PORT                     GPIOA
#define POWER_LED_GPIO_PIN                      GPIO_Pin_10

// PA11 - CPU_WDTO (预留，核心卡喂狗信号输入) 暂不使用
#define CPU_WDTO_GPIO_PORT                      GPIOA
#define CPU_WDTO_GPIO_PIN                       GPIO_Pin_11

// PA12 - SW_BLACK- (预留，亮度-按键输入) 暂不使用
#define SW_BLACK_N_GPIO_PORT                    GPIOA
#define SW_BLACK_N_GPIO_PIN                     GPIO_Pin_12

// PA13 - SWDIO (单片机烧录口)
#define SWDIO_GPIO_PORT                         GPIOA
#define SWDIO_GPIO_PIN                          GPIO_Pin_13

// PA14 - SWCLK (单片机烧录口)
#define SWCLK_GPIO_PORT                         GPIOA
#define SWCLK_GPIO_PIN                          GPIO_Pin_14

// PA15 - PWRSUS_EN (预留，SUS电源使能高有效) 暂不使用
#define PWRSUS_EN_GPIO_PORT                     GPIOA
#define PWRSUS_EN_GPIO_PIN                      GPIO_Pin_15

/******************************************PB*****************************************/
// PB0 - ADC2    5V电压采集，系数4.8
#define ADC2_GPIO_PORT                          GPIOB
#define ADC2_GPIO_PIN                           GPIO_Pin_0

// PB1 - ADC3    3V3电压采集，系数3.2
#define ADC3_GPIO_PORT                          GPIOB
#define ADC3_GPIO_PIN                           GPIO_Pin_1

// PB2 - BOOT1 (启动模式配置脚，硬件/BootROM使用，固件不需要初始化)

// PB3 - PWREN (预留，S0域电源使能高有效) 暂不使用
#define PWREN_GPIO_PORT                         GPIOB
#define PWREN_GPIO_PIN                          GPIO_Pin_3

// PB4 - GD_PWRBTIN# (预留，开关机信号输入，低有效) 暂不使用
#define GD_PWRBTIN_GPIO_PORT                    GPIOB
#define GD_PWRBTIN_GPIO_PIN                     GPIO_Pin_4

// PB5 - PWRBTN_OUT# (预留，给核心卡，延时开关机使用) 暂不使用
#define PWRBTN_OUT_GPIO_PORT                    GPIOB
#define PWRBTN_OUT_GPIO_PIN                     GPIO_Pin_5

// PB6 - P3V3SUS_PG 做输入，P3V3SUS电源PG信号（内部上拉）
#define P3V3SUS_PG_GPIO_PORT                    GPIOB
#define P3V3SUS_PG_GPIO_PIN                     GPIO_Pin_6

// PB7 - P3V3_STBY_PG 做输入，P3V3_STBY电源PG信号（内部上拉）
#define P3V3_STBY_PG_GPIO_PORT                  GPIOB
#define P3V3_STBY_PG_GPIO_PIN                   GPIO_Pin_7

// PB8 - MO_SCL (采集温度，模拟I2C，见 bsp_mo_i2c.h)
#define MO_SCL_GPIO_PORT                        GPIOB
#define MO_SCL_GPIO_PIN                         GPIO_Pin_8

// PB9 - MO_SDA (采集温度，模拟I2C，见 bsp_mo_i2c.h)
#define MO_SDA_GPIO_PORT                        GPIOB
#define MO_SDA_GPIO_PIN                         GPIO_Pin_9

// PB10 - IIC_SCL2 (预留，与核心卡互连IIC) 暂不使用，见 bsp_i2c.h 的 COM_I2C2
// PB11 - IIC_SDA2 (预留，与核心卡互连IIC) 暂不使用，见 bsp_i2c.h 的 COM_I2C2

// PB12 - P12V_EN (预留，核心卡12V电源使能高有效) 暂不使用
#define P12V_EN_GPIO_PORT                       GPIOB
#define P12V_EN_GPIO_PIN                        GPIO_Pin_12

// PB13 - PWROK 底板电源OK信号输出，参考PB6(P3V3SUS_PG)高后输出高
#define PWROK_GPIO_PORT                         GPIOB
#define PWROK_GPIO_PIN                          GPIO_Pin_13

// PB14 - FAN_EN (预留，风扇使能信号高有效) 暂不使用
#define FAN_EN_GPIO_PORT                        GPIOB
#define FAN_EN_GPIO_PIN                         GPIO_Pin_14

// PB15 - SW_PANEL (预留，屏电开关信号输入) 暂不使用
#define SW_PANEL_GPIO_PORT                      GPIOB
#define SW_PANEL_GPIO_PIN                       GPIO_Pin_15

/******************************************PC*****************************************/
// PC0 - SLP_S3# 核心卡S3睡眠信号输入，低有效（内部上拉）
#define SLP_S3_GPIO_PORT                        GPIOC
#define SLP_S3_GPIO_PIN                         GPIO_Pin_0

// PC1 - SLP_S4# 核心卡S4休眠信号输入，低有效（内部上拉）
#define SLP_S4_GPIO_PORT                        GPIOC
#define SLP_S4_GPIO_PIN                         GPIO_Pin_1

// PC2 - SLP_S5# 核心卡S5关机信号输入，低有效（内部上拉）
#define SLP_S5_GPIO_PORT                        GPIOC
#define SLP_S5_GPIO_PIN                         GPIO_Pin_2

// PC3 - PCIE_RESET# (预留，参考PA6信号，低有效) 暂不使用
#define PCIE_RESET_GPIO_PORT                    GPIOC
#define PCIE_RESET_GPIO_PIN                     GPIO_Pin_3

// PC4 - ADC4 MT9221CT_OUT 电流监测（模拟量，由 bsp_adc.h/bsp_adc.c 管理采集）
#define ADC4_GPIO_PORT                           GPIOC
#define ADC4_GPIO_PIN                            GPIO_Pin_4

// PC5 - GDSYS_RST (预留，核心卡复位信号，低有效) 暂不使用，注意：不要拉低该信号

// PC6 - FAN_PWM 做输入，核心卡风扇PWM信号输入 (EXTI6中断实时镜像到PC7，见 bsp_pwm.h)
// PC7 - FAN1_PWM 做输出，实时转发PC6电平给风扇 (见 bsp_pwm.h)

// PC8 - GN32_BL_PWM 屏背光亮度调节输出，高电平有效，参考PB6(P3V3SUS_PG)高后输出高
#define GN32_BL_PWM_GPIO_PORT                   GPIOC
#define GN32_BL_PWM_GPIO_PIN                    GPIO_Pin_8

// PC9 - MCU_GPIO (预留，与核心卡IO互连) 暂不使用
#define MCU_GPIO_GPIO_PORT                      GPIOC
#define MCU_GPIO_GPIO_PIN                       GPIO_Pin_9

// PC10 - DEBUG_TX (调试串口，见 bsp_usart.h)
#define DEBUG_TX_GPIO_PORT                      GPIOC
#define DEBUG_TX_GPIO_PIN                       GPIO_Pin_10

// PC11 - DEBUG_RX
#define DEBUG_RX_GPIO_PORT                      GPIOC
#define DEBUG_RX_GPIO_PIN                       GPIO_Pin_11

// PC12 - SER_RX0 与核心卡串口0互连，健康上报 (UART5_TX，见 bsp_usart.h)

/******************************************PD*****************************************/
// PD2 - SER_TX0 与核心卡串口0互连，健康上报 (UART5_RX，见 bsp_usart.h)

/***********************************input***********************************************/

extern uint8_t line0_low ;

extern xTaskHandle xExitLine0Semaphore;

extern TaskHandle_t GPIO_Task_Handle;

// UART4 debug口收到"Reset"命令后触发自复位脉冲的同步信号量（由 bsp_usart.c 的 ISR Give）
extern SemaphoreHandle_t xSelfResetSemaphore;

void bsp_gpio_init(void);
void exit_gpio_init(void);
void GPIO_Task(void* parameter);

#endif

