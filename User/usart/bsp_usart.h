#ifndef __USART_H
#define	__USART_H


#include "stm32f10x.h"
#include <stdio.h>

/** 
  * ���ں궨�壬��ͬ�Ĵ��ڹ��ص����ߺ�IO��һ������ֲʱ��Ҫ�޸��⼸����
	* 1-�޸�����ʱ�ӵĺ꣬uart1���ص�apb2���ߣ�����uart���ص�apb1����
	* 2-�޸�GPIO�ĺ�
  */

// ����4-UART4
#define  DEBUG_USARTx                   UART4
#define  DEBUG_USART_CLK                RCC_APB1Periph_UART4
#define  DEBUG_USART_APBxClkCmd         RCC_APB1PeriphClockCmd
#define  DEBUG_USART_BAUDRATE           115200

// USART GPIO ���ź궨��
#define  DEBUG_USART_GPIO_CLK           (RCC_APB2Periph_GPIOC)
#define  DEBUG_USART_GPIO_APBxClkCmd    RCC_APB2PeriphClockCmd
    
#define  DEBUG_USART_TX_GPIO_PORT       GPIOC   
#define  DEBUG_USART_TX_GPIO_PIN        GPIO_Pin_10
#define  DEBUG_USART_RX_GPIO_PORT       GPIOC
#define  DEBUG_USART_RX_GPIO_PIN        GPIO_Pin_11

#define  DEBUG_USART_IRQ                UART4_IRQn
#define  DEBUG_USART_IRQHandler         UART4_IRQHandler

// PC12(TX)/PD2(RX) - SER_RX0/SER_TX0，与核心卡串口0互连，健康上报口（对照Sheet3「定义」，即Sheet1标注的UART5）
#define  HEALTH_USARTx                   UART5
#define  HEALTH_USART_CLK                RCC_APB1Periph_UART5
#define  HEALTH_USART_APBxClkCmd         RCC_APB1PeriphClockCmd
#define  HEALTH_USART_BAUDRATE           115200          // 暂定，和debug口一致，具体上报协议待定

#define  HEALTH_USART_TX_GPIO_CLK        (RCC_APB2Periph_GPIOC)
#define  HEALTH_USART_RX_GPIO_CLK        (RCC_APB2Periph_GPIOD)
#define  HEALTH_USART_GPIO_APBxClkCmd    RCC_APB2PeriphClockCmd

#define  HEALTH_USART_TX_GPIO_PORT       GPIOC
#define  HEALTH_USART_TX_GPIO_PIN        GPIO_Pin_12
#define  HEALTH_USART_RX_GPIO_PORT       GPIOD
#define  HEALTH_USART_RX_GPIO_PIN        GPIO_Pin_2

void USART_Config(void);
void Usart_SendByte( USART_TypeDef * pUSARTx, uint8_t ch);
void Usart_SendString( USART_TypeDef * pUSARTx, char *str);
void Usart_SendHalfWord( USART_TypeDef * pUSARTx, uint16_t ch);

// UART5健康上报口：外设初始化 + 发送占位（具体上报内容/协议未定，先只把外设跑起来）
void HEALTH_USART_Config(void);
void UART5_SendHealthReport(char *str);

#endif /* __USART_H */