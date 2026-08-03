#ifndef __BSP_I2C_H
#define	__BSP_I2C_H

#include "bsp_init.h"

// 注意：PB6/PB7 已按 Sheet3「定义」改为 P3V3SUS_PG/P3V3_STBY_PG 数字输入（见 bsp_gpio.h），
// 不再用作硬件 I2C1，因此这里不再定义 COM_I2C1。

#define             COM_I2C2                                 I2C2
#define             COM_I2C2_APBxClock_FUN                   RCC_APB1PeriphClockCmd
#define             COM_I2C2_CLK                             RCC_APB1Periph_I2C2
#define             COM_I2C2_GPIO_APBxClock_FUN              RCC_APB2PeriphClockCmd
#define             COM_I2C2_GPIO_CLK                        RCC_APB2Periph_GPIOB     
#define             COM_I2C2_SCL_PORT                        GPIOB   
#define             COM_I2C2_SCL_PIN                         GPIO_Pin_10
#define             COM_I2C2_SDA_PORT                        GPIOB 
#define             COM_I2C2_SDA_PIN                         GPIO_Pin_11

#define I2C2_OWN_ADDRESS7      0X0B
#define I2C_Speed              400000

/*�ȴ���ʱʱ��*/
#define I2CT_FLAG_TIMEOUT         ((uint32_t)0x1000)
#define I2CT_LONG_TIMEOUT         ((uint32_t)(10 * I2CT_FLAG_TIMEOUT))


extern uint8_t Read_I2C2_Addr[256];
extern  uint16_t I2C2_DataSize;

void init_ipmb_i2c(void);
uint32_t I2C2_Write(uint8_t *Data,uint16_t Num);
void COM_I2C2_Task(void* parameter);

#endif
