#include "task_usart.h"
#include "bsp_init.h"
#include "bsp_i2c.h"
#include <stdio.h>
#include "bsp_mo_i2c.h"
#include "bsp_adc.h"
#include "stm32f10x.h"
#include "stm32f10x_i2c.h"
#include ".\timer\bsp_pwm.h"
#include <string.h>

// 声明pwm_func函数
void pwm_func(void);

TaskHandle_t Sensor_Task_Handle = NULL;



stPRINTF_BUF_t stPrintf_Buf = {0};
stPRINTF_BUF_t stCpu_Buf = {0};
uint8_t power_on_cnt=0;
uint8_t iic_switch=0;

/*
*********************************************************************************************************
*	函 数 名: Sensor_Task
*	功能说明: Sensor_Task任务主体
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
void Sensor_Task(void* parameter)
{
	while (1)
  {
    // 清空打印缓冲区
    memset(stPrintf_Buf.buf, 0, sizeof(stPrintf_Buf.buf));
    
    // 1. 计算电压值
    ADC_Calculation();
    
    // 2. 采集温度
    Board_ADDR90_temp();
    Board_ADDR92_temp();
    Board_ADDR94_temp();
    
    // 3. 获取FAN_PWM/FAN1_TACH转发信号的频率、占空比信息
    pwm_func();

    // 4. 计算风扇转速（RPM），现代风扇通常每转产生2个脉冲
    uint16_t fan_rpm = (uint16_t)((g_fan_tach_relay.have_period && g_fan_tach_relay.period_ticks > 0)
                                   ? ((1000000.0f / g_fan_tach_relay.period_ticks / 2) * 60)
                                   : 0);

    // 5. 打印所有值
    printf("\r\n=================== 系统状态 ===================\r\n");
    printf("固件版本: V1.0  编译时间: %s %s\r\n", __DATE__, __TIME__);
    printf("%s", stPrintf_Buf.buf);

    // 打印风扇转速
    printf("风扇转速: %d RPM\r\n", fan_rpm);

    printf("================================================\r\n");
    
    vTaskDelay(2000);
    }
}



