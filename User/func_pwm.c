#include "bsp_init.h"
#include "timer/bsp_pwm.h"
#include <string.h>

/***********************************************************************
* @ 函数名  pwm_func
* @ 功能说明  上报 FAN_PWM(PC6->PC7)/FAN1_TACH(PA9->PA8) 转发信号的频率、占空比（仅用于诊断打印）
*********************************************************************/
void pwm_func(void)
{
    char flash_buf[128] = {0};

    if (g_fan_pwm_relay.have_period && g_fan_pwm_relay.period_ticks > 0)
    {
        float freq = 1000000.0f / g_fan_pwm_relay.period_ticks;
        float duty = (float)g_fan_pwm_relay.high_ticks * 100.0f / g_fan_pwm_relay.period_ticks;
        sprintf((char *)flash_buf, "FAN_PWM(PC6->PC7):频率：%.1f HZ , 占空比：%.1f%%\r\n", freq, duty);
    }
    else
    {
        sprintf((char *)flash_buf, "FAN_PWM(PC6->PC7):无信号\r\n");
    }
    strcat((char *)stPrintf_Buf.buf, flash_buf);
    memset(flash_buf, 0, sizeof(flash_buf));

    if (g_fan_tach_relay.have_period && g_fan_tach_relay.period_ticks > 0)
    {
        float freq = 1000000.0f / g_fan_tach_relay.period_ticks;
        float duty = (float)g_fan_tach_relay.high_ticks * 100.0f / g_fan_tach_relay.period_ticks;
        sprintf((char *)flash_buf, "FAN1_TACH(PA9->PA8):频率：%.1f HZ , 占空比：%.1f%%\r\n", freq, duty);
    }
    else
    {
        sprintf((char *)flash_buf, "FAN1_TACH(PA9->PA8):无信号\r\n");
    }
    strcat((char *)stPrintf_Buf.buf, flash_buf);
}
