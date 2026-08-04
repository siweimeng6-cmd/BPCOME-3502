#include ".\gpio\bsp_gpio.h"

TaskHandle_t GPIO_Task_Handle;
SemaphoreHandle_t xSelfResetSemaphore = NULL;

void bsp_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // 启用GPIO时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);

    /************************** PA0 - SELF_RST 自复位，高电平有效，空闲拉低 **************************/
    GPIO_InitStructure.GPIO_Pin = SELF_RST_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SELF_RST_GPIO_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(SELF_RST_GPIO_PORT, SELF_RST_GPIO_PIN);

    xSelfResetSemaphore = xSemaphoreCreateBinary();

    /************************** PA6 - CB_RESET# 做输入，核心卡复位输出信号 **************************/
    GPIO_InitStructure.GPIO_Pin = CB_RESET_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(CB_RESET_GPIO_PORT, &GPIO_InitStructure);

    /************************** 电源时序相关引脚 **************************/
    // PB6 - P3V3SUS_PG 做输入，P3V3SUS电源PG信号（内部上拉）
    GPIO_InitStructure.GPIO_Pin = P3V3SUS_PG_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(P3V3SUS_PG_GPIO_PORT, &GPIO_InitStructure);

    // PB7 - P3V3_STBY_PG 做输入，P3V3_STBY电源PG信号（内部上拉）
    GPIO_InitStructure.GPIO_Pin = P3V3_STBY_PG_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(P3V3_STBY_PG_GPIO_PORT, &GPIO_InitStructure);

    // PB13 - PWROK 底板电源OK信号输出，参考PB6高后输出高。初始状态拉低（PB6尚未确认高之前不给电源OK）
    GPIO_InitStructure.GPIO_Pin = PWROK_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PWROK_GPIO_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(PWROK_GPIO_PORT, PWROK_GPIO_PIN);

    // PA4 - GN32_BL_EN 屏背光使能，参考PB6高后输出高。初始状态拉低
    GPIO_InitStructure.GPIO_Pin = GN32_BL_EN_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GN32_BL_EN_GPIO_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(GN32_BL_EN_GPIO_PORT, GN32_BL_EN_GPIO_PIN);

    // PA5 - PANEL_EN_GD 屏供电使能，参考PB6高后输出高。初始状态拉低
    GPIO_InitStructure.GPIO_Pin = PANEL_EN_GD_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PANEL_EN_GD_GPIO_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(PANEL_EN_GD_GPIO_PORT, PANEL_EN_GD_GPIO_PIN);

    // PC8 - GN32_BL_PWM 屏背光亮度调节输出，参考PB6高后输出高。初始状态拉低
    GPIO_InitStructure.GPIO_Pin = GN32_BL_PWM_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GN32_BL_PWM_GPIO_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(GN32_BL_PWM_GPIO_PORT, GN32_BL_PWM_GPIO_PIN);

    /************************** 核心卡睡眠状态输入 **************************/
    // PC0 - SLP_S3# 核心卡S3睡眠信号输入，低有效（内部上拉）
    GPIO_InitStructure.GPIO_Pin = SLP_S3_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(SLP_S3_GPIO_PORT, &GPIO_InitStructure);

    // PC1 - SLP_S4# 核心卡S4休眠信号输入，低有效（内部上拉）
    GPIO_InitStructure.GPIO_Pin = SLP_S4_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(SLP_S4_GPIO_PORT, &GPIO_InitStructure);

    // PC2 - SLP_S5# 核心卡S5关机信号输入，低有效（内部上拉）
    GPIO_InitStructure.GPIO_Pin = SLP_S5_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(SLP_S5_GPIO_PORT, &GPIO_InitStructure);
}

/***********************************************************************
* @ 函数名  report_level_change
* @ 功能说明  读取一个输入脚，电平变化时打印，仅用于调试观察，不做其他联动。
*             *pre_state 传 0xFF 表示还没读过，跳过开机首次读取，避免误打印。
* @ 返回值  无
*********************************************************************/
static void report_level_change(GPIO_TypeDef *port, uint16_t pin, uint8_t *pre_state,
                                 const char *msg_high, const char *msg_low)
{
    uint8_t state = GPIO_ReadInputDataBit(port, pin);

    if(state != *pre_state)
    {
        if(*pre_state != 0xFF)
        {
            printf("%s", (state == Bit_SET) ? msg_high : msg_low);
        }
        *pre_state = state;
    }
}

/***********************************************************************
* @ 函数名  GPIO_Task
* @ 功能说明  GPIO控制任务：PB13(PWROK)/PA4(GN32_BL_EN)/PA5(PANEL_EN_GD)/PC8(GN32_BL_PWM)
*             跟随PB6(P3V3SUS_PG) 状态；同时等待 UART4 收到"Reset"命令后，对
*             PA0(SELF_RST) 做一次高电平100ms的自复位脉冲（高电平有效）。
* @ 参数    parameter: 任务参数
* @ 返回值  无
*********************************************************************/
void GPIO_Task(void* parameter)
{
    uint8_t pre_cb_reset = 0xFF;   // 0xFF表示还没读过，首次只记录不打印
    uint8_t pre_pwrok = 0xFF;
    uint8_t pre_stby_pg = 0xFF;
    uint8_t pre_slp_s3 = 0xFF;
    uint8_t pre_slp_s4 = 0xFF;
    uint8_t pre_slp_s5 = 0xFF;

    while(1)
    {
        // 用信号量当作100ms轮询周期的等待：收到Reset命令会提前唤醒，否则超时后继续走下面的轮询
        if(xSemaphoreTake(xSelfResetSemaphore, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            printf("[SELF_RST] 收到Reset命令，PA0拉高\r\n");
            GPIO_SetBits(SELF_RST_GPIO_PORT, SELF_RST_GPIO_PIN);    // 拉高，触发自复位
            vTaskDelay(pdMS_TO_TICKS(100));
            GPIO_ResetBits(SELF_RST_GPIO_PORT, SELF_RST_GPIO_PIN);   // 拉低，恢复空闲
            printf("[SELF_RST] 100ms后PA0拉低，自复位脉冲结束\r\n");
        }

        // PWROK/GN32_BL_EN/PANEL_EN_GD/GN32_BL_PWM 都直接跟随 P3V3SUS_PG(PB6)：PB6为高则四路都输出高，为低则都输出低
        {
            uint8_t p3v3sus_pg = GPIO_ReadInputDataBit(P3V3SUS_PG_GPIO_PORT, P3V3SUS_PG_GPIO_PIN);

            if(p3v3sus_pg == Bit_SET)
            {
                GPIO_SetBits(PWROK_GPIO_PORT, PWROK_GPIO_PIN);
                GPIO_SetBits(GN32_BL_EN_GPIO_PORT, GN32_BL_EN_GPIO_PIN);
                GPIO_SetBits(PANEL_EN_GD_GPIO_PORT, PANEL_EN_GD_GPIO_PIN);
                GPIO_SetBits(GN32_BL_PWM_GPIO_PORT, GN32_BL_PWM_GPIO_PIN);
            }
            else
            {
                GPIO_ResetBits(PWROK_GPIO_PORT, PWROK_GPIO_PIN);
                GPIO_ResetBits(GN32_BL_EN_GPIO_PORT, GN32_BL_EN_GPIO_PIN);
                GPIO_ResetBits(PANEL_EN_GD_GPIO_PORT, PANEL_EN_GD_GPIO_PIN);
                GPIO_ResetBits(GN32_BL_PWM_GPIO_PORT, GN32_BL_PWM_GPIO_PIN);
            }
        }

        // 各输入信号的电平变化打印（仅用于调试观察，没有其他联动逻辑）
        report_level_change(P3V3SUS_PG_GPIO_PORT, P3V3SUS_PG_GPIO_PIN, &pre_pwrok,
                             "[PWROK] P3V3SUS_PG变高，PB13(PWROK)/PA4(BL_EN)/PA5(PANEL_EN)/PC8(BL_PWM)拉高\r\n",
                             "[PWROK] P3V3SUS_PG变低，PB13(PWROK)/PA4(BL_EN)/PA5(PANEL_EN)/PC8(BL_PWM)拉低\r\n");

        report_level_change(P3V3_STBY_PG_GPIO_PORT, P3V3_STBY_PG_GPIO_PIN, &pre_stby_pg,
                             "[P3V3_STBY_PG] 变高\r\n",
                             "[P3V3_STBY_PG] 变低\r\n");

        report_level_change(CB_RESET_GPIO_PORT, CB_RESET_GPIO_PIN, &pre_cb_reset,
                             "[CB_RESET#] 检测到核心卡复位信号变高（复位释放）\r\n",
                             "[CB_RESET#] 检测到核心卡复位信号变低（复位中）\r\n");

        report_level_change(SLP_S3_GPIO_PORT, SLP_S3_GPIO_PIN, &pre_slp_s3,
                             "[SLP_S3#] 变高（退出S3睡眠）\r\n",
                             "[SLP_S3#] 变低（进入S3睡眠）\r\n");

        report_level_change(SLP_S4_GPIO_PORT, SLP_S4_GPIO_PIN, &pre_slp_s4,
                             "[SLP_S4#] 变高（退出S4休眠）\r\n",
                             "[SLP_S4#] 变低（进入S4休眠）\r\n");

        report_level_change(SLP_S5_GPIO_PORT, SLP_S5_GPIO_PIN, &pre_slp_s5,
                             "[SLP_S5#] 变高（退出S5关机）\r\n",
                             "[SLP_S5#] 变低（进入S5关机）\r\n");
    }
}
