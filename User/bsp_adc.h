#ifndef __ADC_H
#define	__ADC_H


#include "stm32f10x.h"

// 注意：用作ADC采集的IO必须没有复用，否则采集电压会有影响
/********************ADC1输入通道（引脚）配置**************************/
#define    ADC_APBxClock_FUN             RCC_APB2PeriphClockCmd
#define    ADC_CLK                       RCC_APB2Periph_ADC1

#define    ADC_GPIO_APBxClock_FUN        RCC_APB2PeriphClockCmd


// 注意
// 1-PC0 在霸道里面接的是蜂鸣器，默认被拉低
// 2-PC0 在指南者里面接的是SPI FLASH的 片选，默认被拉高
// 所以 PC0 做 ADC 转换通道的时候，结果可能会有误差

// 转换通道个数
#define 	 ADC_CONVERT_CHANNEL      			4									//转换通道数
#define 	 MAX_ADC_COLL_CNTS							20									//转换轮数
#define 	 ADC_VREF                       3.3f      							//3.3V
#define 	 ADC_PRE_12BIT                  4096.0f   							//2^12


// PA7 - ADC1 P12V电压采集，系数12
#define    ADC_CNT1                      GPIO_Pin_7
#define    ADC_CNT1_PORT                 GPIOA
#define    ADC_CNT1_CHANNEL              ADC_Channel_7

// PB0 - ADC2    P5V电压采集，系数4.8
#define    ADC_CNT2                      GPIO_Pin_0
#define    ADC_CNT2_PORT                 GPIOB
#define    ADC_CNT2_CHANNEL              ADC_Channel_8

// PB1 - ADC3    P3V3电压采集，系数3.2
#define    ADC_CNT3                      GPIO_Pin_1
#define    ADC_CNT3_PORT                 GPIOB
#define    ADC_CNT3_CHANNEL              ADC_Channel_9

// PC4 - ADC4    MT9221CT_OUT 电流监测
#define    ADC_CNT4                      GPIO_Pin_4
#define    ADC_CNT4_PORT                 GPIOC
#define    ADC_CNT4_CHANNEL              ADC_Channel_14

// MT9221电流检测芯片换算参数：VCC=5V，DC(单向)模式，敏感度0.2V/A(型号SNST_10BR5_20UR5)，效率90%
#define    MT9221_SUPPLY_VCC             5.0f
#define    MT9221_VOQ_DC                 0.1f
#define    MT9221_VOL_BASE               (MT9221_VOQ_DC * MT9221_SUPPLY_VCC)   // 0.5V
#define    MT9221_SENSITIVITY            0.2f
#define    MT9221_EFFICIENCY             0.9f



// ADC1 对应 DMA1通道1，ADC3对应DMA2通道5，ADC2没有DMA功能
#define    ADC_x                         ADC1
#define    ADC_DMA_CHANNEL               DMA1_Channel1
#define    ADC_DMA_CLK                   RCC_AHBPeriph_DMA1

typedef struct
{
  volatile uint32_t adc_filter_val[ADC_CONVERT_CHANNEL];																	
  volatile float adc_voltage_val[ADC_CONVERT_CHANNEL];						
}stADC_DATA_t, *pstADC_DATA_t;

// ADC采集结果，adc_voltage_val[0..2]为P12V/P5V/P3V3电压(V)，[3]为PC4电流(A)
extern stADC_DATA_t stADC_Data;


/**************************函数声明********************************/
void ADCx_Init(void);
void adc_convert(void);
void ADC_Calculation(void);


#endif /* __ADC_H */
