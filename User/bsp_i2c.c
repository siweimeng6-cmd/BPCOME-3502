#include "bsp_i2c.h"
#include "bsp_init.h"


uint8_t I2C2_Flag = 0;
uint16_t I2C2_DataSize = 0;
uint8_t Write_I2C2_Addr[256];
uint8_t Read_I2C2_Addr[256];

static __IO uint32_t  I2CTimeout = I2CT_LONG_TIMEOUT;
static  uint32_t I2C2_TIMEOUT_UserCallback(uint8_t errorCode);

xTaskHandle xi2c2Semaphore     = NULL;

/*
*********************************************************************************************************
*	函 数 名: I2C_GPIO_Config
*	功能说明: 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void I2C_GPIO_Config(void)
{
  GPIO_InitTypeDef  GPIO_InitStructure; 

	/* 使能与 I2C 有关的时钟 */
	RCC_APB1PeriphClockCmd ( COM_I2C2_CLK, ENABLE );
	RCC_APB2PeriphClockCmd ( RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE );
	
    
  /* I2C_SCL、I2C_SDA*/
  GPIO_InitStructure.GPIO_Pin = COM_I2C2_SCL_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;	       // 开漏输出
  GPIO_Init(GPIOB, &GPIO_InitStructure);
	
  GPIO_InitStructure.GPIO_Pin = COM_I2C2_SDA_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;	       // 开漏输出
  GPIO_Init(GPIOB, &GPIO_InitStructure);	
	
	
}


/*
*********************************************************************************************************
*	函 数 名: I2C_Mode_Configu
*	功能说明: 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void I2C_Mode_Configu(void)
{
  I2C_InitTypeDef  I2C_InitStructure; 


  I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
  I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
  I2C_InitStructure.I2C_OwnAddress1 =I2C2_OWN_ADDRESS7;
  I2C_InitStructure.I2C_Ack = I2C_Ack_Enable ;
  I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
  I2C_InitStructure.I2C_ClockSpeed = I2C_Speed;

  I2C_Init(COM_I2C2, &I2C_InitStructure);
  I2C_Cmd(COM_I2C2, ENABLE);

	//I2C_ITConfig(COM_I2C2, I2C_IT_BUF | I2C_IT_EVT | I2C_IT_ERR, ENABLE);
}


/*
*********************************************************************************************************
*	函 数 名: init_ipmb_i2c
*	功能说明: 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void init_ipmb_i2c(void)
{
		I2C_GPIO_Config();
		I2C_Mode_Configu();
}

/*
*********************************************************************************************************
*	函 数 名: I2C2_Write
*	功能说明:
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
uint32_t I2C2_Write(uint8_t *Data,uint16_t Num)
{
	I2CTimeout = I2CT_FLAG_TIMEOUT;	
	while(I2C_GetFlagStatus(I2C2,I2C_FLAG_BUSY) != ERROR)                              // 等到上一操作结束
	{
    if((I2CTimeout--) == 0) return I2C2_TIMEOUT_UserCallback(0);
  }  
	I2C_GenerateSTART(I2C2,ENABLE);                                                     // 产生开始信号
	
	I2CTimeout = I2CT_FLAG_TIMEOUT;	
	while(I2C_CheckEvent(I2C2,I2C_EVENT_MASTER_MODE_SELECT) == ERROR)                  // 检查标志位，确认是否已经产生
	{
    if((I2CTimeout--) == 0) return I2C2_TIMEOUT_UserCallback(1);
  }   
	I2C_Send7bitAddress(I2C2,I2C2_OWN_ADDRESS7,I2C_Direction_Transmitter);                // 发送 7 位设备地址，方向为写方向
	
	I2CTimeout = I2CT_FLAG_TIMEOUT;	
	while(I2C_CheckEvent(I2C2,I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == ERROR)    // 检查标志位，确认是否已经产生
	{
    if((I2CTimeout--) == 0) return I2C2_TIMEOUT_UserCallback(2);
  }   
	while(Num)
	{
		I2C_SendData(I2C2,*Data);                                                       // 地址 自加
		Data++;                                                                         // 数据 自加
		Num--;                                                                          // 要写入的数据大小 自减
		if(Num)                                                                         // 判断是否为最后一个数据，以检查不同的标志位
		{	
			I2CTimeout = I2CT_FLAG_TIMEOUT;	
			while(I2C_CheckEvent(I2C2,I2C_EVENT_MASTER_BYTE_TRANSMITTING) == ERROR)
			{
			if((I2CTimeout--) == 0) return I2C2_TIMEOUT_UserCallback(3);
			}	
		}			
		else	
		{		
			I2CTimeout = I2CT_FLAG_TIMEOUT;	
			while(I2C_CheckEvent(I2C2,I2C_EVENT_MASTER_BYTE_TRANSMITTED) == ERROR)	
			{
			if((I2CTimeout--) == 0) return I2C2_TIMEOUT_UserCallback(4);
			} 
		}
	}
	I2C_GenerateSTOP(I2C2,ENABLE);                                                      // 产生结束信号
} 

/*
*********************************************************************************************************
*	函 数 名: I2C2_EV_IRQHandler
*	功能说明: 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void I2C2_EV_IRQHandler(void)
{
		BaseType_t  xHigherPriorityTaskWoken;
		if(I2C_GetFlagStatus(I2C2,I2C_FLAG_MSL) == ERROR)                                       // 检测 I2C2 是否为从机
		{
				if(I2C_CheckEvent(I2C2,I2C_EVENT_SLAVE_RECEIVER_ADDRESS_MATCHED) != ERROR) 
				{
					I2C2_DataSize = 0;
				}
				if(I2C_CheckEvent(I2C2,I2C_EVENT_SLAVE_BYTE_RECEIVED) != ERROR)                     // 检测到数据,接收数据
				{
					Read_I2C2_Addr[I2C2_DataSize]	= I2C_ReceiveData(I2C2);                        // 接收数据
					I2C2_DataSize++;
				}
				if(I2C_CheckEvent(I2C2,I2C_EVENT_SLAVE_STOP_DETECTED) != ERROR)                     // 检测停止位
				{
					I2C_Cmd(I2C2,ENABLE);																													// 清除标志				
					xHigherPriorityTaskWoken = pdFALSE;/* 释放二值信号量 */	
					xSemaphoreGiveFromISR(xi2c2Semaphore, &xHigherPriorityTaskWoken);	/* 发送同步信号 */
					/* 如果xHigherPriorityTaskWoken = pdTRUE,那么退出中断后切到当前最高优先级任务执行 */
					portYIELD_FROM_ISR(xHigherPriorityTaskWoken);																															// 设置接收完成标志
				}		
		}	
	
}

/*
*********************************************************************************************************
*	函 数 名: I2C2_ER_IRQHandler
*	功能说明: 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void I2C2_ER_IRQHandler(void)
{
	    if (I2C_GetITStatus(I2C2, I2C_IT_BERR)) {
        I2C_ClearITPendingBit(I2C2, I2C_IT_BERR);
    }
    if (I2C_GetITStatus(I2C2, I2C_IT_ARLO)) {
        I2C_ClearITPendingBit(I2C2, I2C_IT_ARLO);
    }
    if (I2C_GetITStatus(I2C2, I2C_IT_AF)) {
        I2C_ClearITPendingBit(I2C2, I2C_IT_AF);
    }
    if (I2C_GetITStatus(I2C2, I2C_IT_OVR)) {
        I2C_ClearITPendingBit(I2C2, I2C_IT_OVR);
    }
}

/*
*********************************************************************************************************
*	函 数 名: I2C2_TIMEOUT_UserCallback
*	功能说明: I2C2超时反馈
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static  uint32_t I2C2_TIMEOUT_UserCallback(uint8_t errorCode)
{
  /* Block communication and all processes */
  printf("I2C2 等待超时!errorCode = %d",errorCode);
	  /* Clear AF flag */
  I2C_ClearFlag(I2C2, I2C_FLAG_AF);
    /* STOP condition */    
  I2C_GenerateSTOP(I2C2, ENABLE); 
  
  return 0;
}
/*********************************************END OF FILE**********************/

