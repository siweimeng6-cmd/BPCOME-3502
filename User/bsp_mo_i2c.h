#ifndef __BSP_MO_I2C_H
#define	__BSP_MO_I2C_H

#include "stm32f10x.h"
#include <stdbool.h>
#include <stdio.h>


#define MO_IIC                        1
#define TEST_IIC1_MONI                0			//使用PB6,PB7做模拟IIC，测试隔离芯片
/****************************MO IIC************************************/
/* 定义I2C总线连接的GPIO端口, 用户只需要修改下面4行代码即可任意改变SCL和SDA的引脚 */

#if TEST_IIC1_MONI			//使用PB6,PB7做模拟IIC，测试隔离芯片
#define MO_I2C_GPIO_PORT			GPIOB			/* GPIO端口 */
#define MO_RCC_I2C_PORT 			RCC_APB2Periph_GPIOB		/* GPIO端口时钟 */
#define MO_I2C_SCL_PIN				GPIO_Pin_6			/* 连接到SCL时钟线的GPIO */
#define MO_I2C_SDA_PIN				GPIO_Pin_7			/* 连接到SDA数据线的GPIO */
#else
#define MO_I2C_GPIO_PORT			GPIOB			/* GPIO端口 */
#define MO_RCC_I2C_PORT 			RCC_APB2Periph_GPIOB		/* GPIO端口时钟 */
#define MO_I2C_SCL_PIN				GPIO_Pin_8			/* 连接到SCL时钟线的GPIO */
#define MO_I2C_SDA_PIN				GPIO_Pin_9			/* 连接到SDA数据线的GPIO */
#endif


/* 定义读写SCL和SDA的宏，已增加代码的可移植性和可阅读性 */
#define I2C_SCL_1()  GPIO_SetBits(MO_I2C_GPIO_PORT, MO_I2C_SCL_PIN)		/* SCL = 1 */
#define I2C_SCL_0()  GPIO_ResetBits(MO_I2C_GPIO_PORT,MO_I2C_SCL_PIN)		/* SCL = 0 */

#define I2C_SDA_1()  GPIO_SetBits(MO_I2C_GPIO_PORT, MO_I2C_SDA_PIN )		/* SDA = 1 */
#define I2C_SDA_0()  GPIO_ResetBits(MO_I2C_GPIO_PORT, MO_I2C_SDA_PIN )		/* SDA = 0 */

#define I2C_SDA_READ()  GPIO_ReadInputDataBit(MO_I2C_GPIO_PORT, MO_I2C_SDA_PIN)	/* 读SDA口线状态 */
#define I2C_SCL_READ()  GPIO_ReadInputDataBit(MO_I2C_GPIO_PORT, MO_I2C_SCL_PIN)	/* 读SDA口线状态 */

#define I2C_WR	0		/* 写控制bit */
#define I2C_RD	1		/* 读控制bit */

/****************************硬件IIC************************************/
#define COM_I2C3																	I2C1
#define COM_I2C_EPD20550_APBxClock_FUN            RCC_APB1PeriphClockCmd
#define COM_I2C_EPD20550_CLK                      RCC_APB1Periph_I2C1
#define COM_I2C_EPD20550_GPIO_APBxClock_FUN       RCC_APB2PeriphClockCmd
#define COM_I2C_EPD20550_GPIO_CLK                 RCC_APB2Periph_GPIOB     
#define COM_I2C_EPD20550_SCL_PORT                 GPIOB   
#define COM_I2C_EPD20550_SCL_PIN                  GPIO_Pin_8
#define COM_I2C_EPD20550_SDA_PORT                 GPIOB 
#define COM_I2C_EPD20550_SDA_PIN                  GPIO_Pin_9
#define I2CT_FLAG_TIMEOUT         				  (uint32_t)0x10000)
#define I2CT_LONG_TIMEOUT         				  ((uint32_t)(10 * I2CT_FLAG_TIMEOUT))



/*********************温度传感器**********************************/
//LM75A的地址为7位地址	1		0		0		1 	A2	A1	A0	R/W
#define BOARD_LM75A90_ADDRESS  0x90
#define BOARD_LM75A92_ADDRESS	 0x92
#define BOARD_LM75A94_ADDRESS	 0x94

/*********************EEPROM（与MO_SCL/MO_SDA共用同一路模拟I2C）**********************/
#define EE_MODEL_NAME		"AT24C512"
#define EE_DEV_ADDR			0xA0		        /* 设备地址,其中地址A2、A1、A0为器件地址*/
#define EE_PAGE_SIZE		128		  	      /* 页面大小(字节) */
#define EE_SIZE				 (64*1024)     	  /* 总容量(字节) */
#define EE_ADDR_BYTES		2			          /* 地址字节个数 */

#define BPD20550_ADDRESS									0x80
#define BPD20550_OPERATION								0x01
#define BPD20550_READ_CURRENT							0x8C
#define BPD20550_READ_VOLTAGE_OUT							0x8B
#define BPD20550_READ_VOLTAGE_IN							0x88
#define BPD20550_READ_TEMP								0x8D

extern int board_temp0[2];           //存储整数化后的实际温度值
extern int board_temp1[2];           //存储整数化后的实际温度值
extern int board_temp2[2];           //存储整数化后的实际温度值
extern int board_temp_valid[3];      //三路温度本次读取是否成功，0表示传感器读失败(board_tempN为旧值)



void Init_MO_I2C(void);
void Init_HARD_I2C(void);
void i2c_Start(void);
void i2c_Stop(void);
void i2c_SendByte(uint8_t _ucByte);
uint8_t i2c_ReadByte(void);
uint8_t i2c_WaitAck(void);
void i2c_Ack(void);
void i2c_NAck(void);
uint8_t i2c_CheckDevice(uint8_t _Address);
uint8_t ee_CheckOk(void);
uint8_t ee_ReadBytes(uint8_t *_pReadBuf, uint16_t _usAddress, uint16_t _usSize);
uint8_t ee_WriteBytes(uint8_t *_pWriteBuf, uint16_t _usAddress, uint16_t _usSize);
uint8_t eeprom_test(void);
void Board_ADDR90_temp(void);
void Board_ADDR92_temp(void);
void Board_ADDR94_temp(void);
float temp_calculate(uint8_t buf[2]);
int decimal_2_integer(float a,int b[2]);
void Board_BPD20550_current(void);
uint8_t board_lm75a_temp_ReadBytes(uint8_t *_pReadBuf, uint16_t _usAddress, uint16_t _usSize, uint8_t board_Lm75a_Address);



#endif
