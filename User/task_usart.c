#include "task_usart.h"
#include "bsp_init.h"
#include "bsp_i2c.h"
#include <stdio.h>
#include "bsp_mo_i2c.h"
#include "bsp_adc.h"
#include "stm32f10x.h"
#include "stm32f10x_i2c.h"
#include ".\timer\bsp_pwm.h"
#include ".\usart\bsp_usart.h"
#include ".\gpio\bsp_gpio.h"
#include <string.h>

// 声明pwm_func函数
void pwm_func(void);

TaskHandle_t Sensor_Task_Handle = NULL;
TaskHandle_t UART5_Task_Handle = NULL;

// UART5健康上报的报文缓冲区，做成static全局避免占用任务栈（实际报文约250字节）
static char health_buf[512];

/*
*********************************************************************************************************
*	函 数 名: Health_AppendTemp
*	功能说明: 往健康上报报文里追加一路温度。传感器读失败时上报N/A，
*	         而不是发board_tempN里残留的上一次旧值，避免核心卡把掉线当正常。
*	形    参: dst-目标报文缓冲区  tag-温度标签  valid-本次读取是否成功  temp-整数化温度值
*	返 回 值: 无
*********************************************************************************************************
*/
static void Health_AppendTemp(char *dst, const char *tag, int valid, int temp[2])
{
    char flash_buf[64] = {0};

    if(valid)
        sprintf(flash_buf, "%s:%d.%d C\r\n", tag, temp[0], temp[1]);
    else
        sprintf(flash_buf, "%s:N/A\r\n", tag);

    strcat(dst, flash_buf);
}



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

/*
*********************************************************************************************************
*	函 数 名: UART5_Task
*	功能说明: UART5(PC12/PD2)健康上报任务主体，每2秒向核心卡发送一帧板级状态报文。
*	         本任务只是数据消费者，ADC/温度由Sensor_Task负责刷新（同为2秒周期），
*	         两个任务相位独立，上报数据最多滞后一个采集周期。
*	         报文标签统一用ASCII，避免跨设备链路的中文编码问题。
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
void UART5_Task(void* parameter)
{
    char flash_buf[128] = {0};

    while (1)
    {
        // 清空报文缓冲区
        memset(health_buf, 0, sizeof(health_buf));

        sprintf(flash_buf, "=========== HEALTH REPORT ===========\r\n");
        strcat(health_buf, flash_buf);

        // 1. 电压/电流（来自Sensor_Task刷新的stADC_Data，[3]为PC4电流，单位A）
        sprintf(flash_buf, "V_12V:%.2fV\r\nV_5V:%.2fV\r\nV_3.3V:%.2fV\r\nI_CURRENT:%.2fA\r\n",
                 stADC_Data.adc_voltage_val[0], stADC_Data.adc_voltage_val[1],
                 stADC_Data.adc_voltage_val[2], stADC_Data.adc_voltage_val[3]);
        strcat(health_buf, flash_buf);

        // 2. 三路温度（[0]为整数部分，[1]为两位小数），读失败的那路上报N/A
        Health_AppendTemp(health_buf, "Addr0x90", board_temp_valid[0], board_temp0);
        Health_AppendTemp(health_buf, "Addr0x92", board_temp_valid[1], board_temp1);
        Health_AppendTemp(health_buf, "Addr0x94", board_temp_valid[2], board_temp2);

        // 3. 风扇转速，换算方式与Sensor_Task一致（每转2个脉冲）
        sprintf(flash_buf, "FAN_RPM:%d\r\n",
                 (uint16_t)((g_fan_tach_relay.have_period && g_fan_tach_relay.period_ticks > 0)
                             ? ((1000000.0f / g_fan_tach_relay.period_ticks / 2) * 60)
                             : 0));
        strcat(health_buf, flash_buf);

        // 4. 电源/复位相关GPIO状态，现场读取（PWROK是输出脚，要读输出寄存器）
        sprintf(flash_buf, "P3V3SUS_PG:%d P3V3_STBY_PG:%d PWROK:%d\r\n",
                 GPIO_ReadInputDataBit(P3V3SUS_PG_GPIO_PORT, P3V3SUS_PG_GPIO_PIN),
                 GPIO_ReadInputDataBit(P3V3_STBY_PG_GPIO_PORT, P3V3_STBY_PG_GPIO_PIN),
                 GPIO_ReadOutputDataBit(PWROK_GPIO_PORT, PWROK_GPIO_PIN));
        strcat(health_buf, flash_buf);

        // 5. 核心卡复位/睡眠状态输入
        sprintf(flash_buf, "CB_RESET#:%d SLP_S3#:%d SLP_S4#:%d SLP_S5#:%d\r\n",
                 GPIO_ReadInputDataBit(CB_RESET_GPIO_PORT, CB_RESET_GPIO_PIN),
                 GPIO_ReadInputDataBit(SLP_S3_GPIO_PORT, SLP_S3_GPIO_PIN),
                 GPIO_ReadInputDataBit(SLP_S4_GPIO_PORT, SLP_S4_GPIO_PIN),
                 GPIO_ReadInputDataBit(SLP_S5_GPIO_PORT, SLP_S5_GPIO_PIN));
        strcat(health_buf, flash_buf);

        sprintf(flash_buf, "=====================================\r\n");
        strcat(health_buf, flash_buf);

        UART5_SendHealthReport(health_buf);

        vTaskDelay(2000);
    }
}



