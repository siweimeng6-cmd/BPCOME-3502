#include "bsp_usart.h"
#include "gpio/bsp_gpio.h"
#include <string.h>

#define RESET_CMD   "Reset"


void USART_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // 打开串口GPIO的时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    // 打开串口外设的时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);

    // 将USART Tx的GPIO配置为推挽复用模式
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // 将USART Rx的GPIO配置为浮空输入模式
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // 配置串口的工作参数
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(UART4, &USART_InitStructure);

    // 使能串口
    USART_Cmd(UART4, ENABLE);

    // 使能接收中断，用于监听"Reset"命令（见 DEBUG_USART_IRQHandler）
    USART_ITConfig(UART4, USART_IT_RXNE, ENABLE);
}

/***********************************************************************
* @ 函数名  DEBUG_USART_IRQHandler (即 UART4_IRQHandler)
* @ 功能说明  逐字节接收，按行（\r或\n结尾）匹配"Reset"命令，匹配到则
*             Give xSelfResetSemaphore，交给 GPIO_Task 去做PA0(SELF_RST)
*             的低电平100ms自复位脉冲。
*********************************************************************/
void DEBUG_USART_IRQHandler(void)
{
    static char line_buf[16];
    static uint8_t line_idx = 0;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (USART_GetITStatus(DEBUG_USARTx, USART_IT_RXNE) != RESET)
    {
        uint8_t ch = (uint8_t)USART_ReceiveData(DEBUG_USARTx);

        if (ch == '\r' || ch == '\n')
        {
            line_buf[line_idx] = '\0';
            if (line_idx > 0 && strcmp(line_buf, RESET_CMD) == 0)
            {
                xSemaphoreGiveFromISR(xSelfResetSemaphore, &xHigherPriorityTaskWoken);
            }
            line_idx = 0;
        }
        else if (line_idx < sizeof(line_buf) - 1)
        {
            line_buf[line_idx++] = (char)ch;
        }
        else
        {
            // 行缓冲溢出，丢弃重新开始
            line_idx = 0;
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/***********************************************************************
* @ 函数名  HEALTH_USART_Config
* @ 功能说明  PC12(TX)/PD2(RX) UART5 初始化：与核心卡串口0互连，健康上报口
*             （对照Sheet3「定义」SER_RX0/SER_TX0，Sheet1标注为UART5）。
*             具体上报的数据内容/协议Sheet3未给出，这里只把外设跑起来，
*             发送内容由 UART5_SendHealthReport() 的调用方决定。
*********************************************************************/
void HEALTH_USART_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // 打开串口GPIO的时钟
    RCC_APB2PeriphClockCmd(HEALTH_USART_TX_GPIO_CLK | HEALTH_USART_RX_GPIO_CLK, ENABLE);
    // 打开串口外设的时钟
    RCC_APB1PeriphClockCmd(HEALTH_USART_CLK, ENABLE);

    // PC12 - TX 推挽复用
    GPIO_InitStructure.GPIO_Pin = HEALTH_USART_TX_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(HEALTH_USART_TX_GPIO_PORT, &GPIO_InitStructure);

    // PD2 - RX 浮空输入
    GPIO_InitStructure.GPIO_Pin = HEALTH_USART_RX_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(HEALTH_USART_RX_GPIO_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = HEALTH_USART_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(HEALTH_USARTx, &USART_InitStructure);

    USART_Cmd(HEALTH_USARTx, ENABLE);
}

/***********************************************************************
* @ 函数名  UART5_SendHealthReport
* @ 功能说明  向核心卡发送健康上报内容（占位：具体协议/内容待定，先按字符串发送）
*********************************************************************/
void UART5_SendHealthReport(char *str)
{
    Usart_SendString(HEALTH_USARTx, str);
}

// 重定向printf函数
int fputc(int ch, FILE *f)
{
    USART_SendData(UART4, (uint8_t)ch);
    while (USART_GetFlagStatus(UART4, USART_FLAG_TXE) == RESET);
    return (ch);
}

/*****************  发送一个字节 **********************/
void Usart_SendByte(USART_TypeDef * pUSARTx, uint8_t ch)
{
    /* 发送一个字节数据到USART */
    USART_SendData(pUSARTx, ch);
        
    /* 等待发送数据寄存器为空 */
    while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);	
}

/****************** 发送8位的数组 ************************/
void Usart_SendArray(USART_TypeDef * pUSARTx, uint8_t *array, uint16_t num)
{
    uint8_t i;
	
    for(i=0; i<num; i++)
    {
        /* 发送一个字节数据到USART */
        Usart_SendByte(pUSARTx, array[i]);	
    }
    /* 等待发送完成 */
    while(USART_GetFlagStatus(pUSARTx, USART_FLAG_TC) == RESET);
}

/*****************  发送字符串 **********************/
void Usart_SendString(USART_TypeDef * pUSARTx, char *str)
{
    unsigned int k=0;
    do 
    {
        Usart_SendByte(pUSARTx, *(str + k));
        k++;
    } while(*(str + k) != '\0');
  
    /* 等待发送完成 */
    while(USART_GetFlagStatus(pUSARTx, USART_FLAG_TC) == RESET)
    {}
}

/*****************  发送一个16位的数据 **********************/
void Usart_SendHalfWord(USART_TypeDef * pUSARTx, uint16_t ch)
{
    uint8_t temp_h, temp_l;
	
    /* 取高八位 */
    temp_h = (ch & 0XFF00) >> 8;
    /* 取低八位 */
    temp_l = ch & 0XFF;
	
    /* 发送高八位 */
    USART_SendData(pUSARTx, temp_h);	
    while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);
	
    /* 发送低八位 */
    USART_SendData(pUSARTx, temp_l);	
    while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);	
}



///重定向c库函数scanf到串口，重写向后可使用scanf、getchar等函数
int fgetc(FILE *f)
{
    /* 等待串口输入数据 */
    while (USART_GetFlagStatus(DEBUG_USARTx, USART_FLAG_RXNE) == RESET);

    return (int)USART_ReceiveData(DEBUG_USARTx);
}

