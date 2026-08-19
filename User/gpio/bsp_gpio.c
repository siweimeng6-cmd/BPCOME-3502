#include ".\gpio\bsp_gpio.h"

TaskHandle_t GPIO_Task_Handle;
SemaphoreHandle_t xSelfResetSemaphore = NULL;

void bsp_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // 启用GPIO时钟 + AFIO时钟（重映射寄存器需要）
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD |
                           RCC_APB2Periph_AFIO, ENABLE);

    // 关闭JTAG-DP，只保留SWD：STM32F10x复位后默认PA15(JTDI)/PB3(JTDO)/PB4(NJTRST)
    // 被JTAG调试口占用，不重映射的话这三个脚配置成普通GPIO也不会真正生效。
    // PA13(SWDIO)/PA14(SWCLK)不受影响，SWD烧录调试照常可用。
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

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

    /************************** 开关机按键转发 **************************/
    // PB4 - GD_PWRBTIN# 开关机信号输入，低有效（浮空输入），由GPIO_Task轮询消抖
    GPIO_InitStructure.GPIO_Pin = GD_PWRBTIN_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GD_PWRBTIN_GPIO_PORT, &GPIO_InitStructure);

    // PB5 - PWRBTN_OUT# 做输出给核心卡，低电平有效。初始状态拉高（空闲）
    GPIO_InitStructure.GPIO_Pin = PWRBTN_OUT_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PWRBTN_OUT_GPIO_PORT, &GPIO_InitStructure);
    GPIO_SetBits(PWRBTN_OUT_GPIO_PORT, PWRBTN_OUT_GPIO_PIN);

    /************************** 电源时序相关引脚 **************************/
    // PB6 - P3V3SUS_PG 做输入，P3V3SUS电源PG信号（浮空输入）
    GPIO_InitStructure.GPIO_Pin = P3V3SUS_PG_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(P3V3SUS_PG_GPIO_PORT, &GPIO_InitStructure);

    // PB7 - P3V3_STBY_PG 做输入，P3V3_STBY电源PG信号（浮空输入）
    GPIO_InitStructure.GPIO_Pin = P3V3_STBY_PG_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(P3V3_STBY_PG_GPIO_PORT, &GPIO_InitStructure);

    // PB13 - PWROK 底板电源OK信号输出，参考PC0高后输出高。初始状态拉低
    GPIO_InitStructure.GPIO_Pin = PWROK_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PWROK_GPIO_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(PWROK_GPIO_PORT, PWROK_GPIO_PIN);

    // PB3 - PWREN S0域电源使能，参考PC0高后输出高。初始状态拉低
    GPIO_InitStructure.GPIO_Pin = PWREN_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PWREN_GPIO_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(PWREN_GPIO_PORT, PWREN_GPIO_PIN);

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

    // PA15 - PWRSUS_EN SUS电源使能，参考PC0高后输出高。初始状态拉低
    GPIO_InitStructure.GPIO_Pin = PWRSUS_EN_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PWRSUS_EN_GPIO_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(PWRSUS_EN_GPIO_PORT, PWRSUS_EN_GPIO_PIN);

    /************************** 核心卡睡眠状态输入 **************************/
    // PC0 - SLP_S3# 核心卡开机自检信号，高电平=开机，低电平=关机（浮空输入）
    GPIO_InitStructure.GPIO_Pin = SLP_S3_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(SLP_S3_GPIO_PORT, &GPIO_InitStructure);

    // PC1 - SLP_S4# 核心卡S4休眠信号输入，低有效（浮空输入）
    GPIO_InitStructure.GPIO_Pin = SLP_S4_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(SLP_S4_GPIO_PORT, &GPIO_InitStructure);

    // PC2 - SLP_S5# 核心卡S5关机信号输入，低有效（浮空输入）
    GPIO_InitStructure.GPIO_Pin = SLP_S5_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
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
* @ 功能说明  GPIO控制任务，轮询周期GPIO_TASK_POLL_MS：
*             1) PA4(GN32_BL_EN)/PA5(PANEL_EN_GD)/PC8(GN32_BL_PWM) 跟随
*                PB6(P3V3SUS_PG) 状态，PA15(PWRSUS_EN)/PB13(PWROK)/PB3(PWREN)
*                跟随 PC0(SLP_S3#) 状态；
*             2) 轮询消抖PB4(GD_PWRBTIN#)，连续采到低电平满20ms就给核心卡转发
*                一个200ms低脉冲(PB5/PWRBTN_OUT#)；
*             3) 等待 UART4 收到"Reset"命令后，对PA0(SELF_RST) 做一次高电平100ms
*                的自复位脉冲（高电平有效）。
* @ 参数    parameter: 任务参数
* @ 返回值  无
*********************************************************************/
void GPIO_Task(void* parameter)
{
    uint8_t pre_cb_reset = 0xFF;   // 0xFF表示还没读过，首次只记录不打印
    uint8_t pre_sus_pg = 0xFF;
    uint8_t pre_stby_pg = 0xFF;
    uint8_t pre_slp_s3 = 0xFF;
    uint8_t pre_slp_s4 = 0xFF;
    uint8_t pre_slp_s5 = 0xFF;
    uint8_t pwrbtn_low_cnt = 0;    // PB4连续采到低电平的次数
    uint8_t pwrbtn_fired = 0;      // 本次按下是否已经转发过脉冲，松开后清零重新武装
    uint8_t pre_pc0_follow = 0xFF; // 调试用：PC0跟随块自己的边沿判断，独立于pre_slp_s3

    while(1)
    {
        // 用信号量当作轮询周期的等待：收到Reset命令会提前唤醒，否则超时后继续走下面的轮询
        if(xSemaphoreTake(xSelfResetSemaphore, pdMS_TO_TICKS(GPIO_TASK_POLL_MS)) == pdTRUE)
        {
            printf("[SELF_RST] 收到Reset命令，PA0拉高\r\n");
            GPIO_SetBits(SELF_RST_GPIO_PORT, SELF_RST_GPIO_PIN);    // 拉高，触发自复位
            vTaskDelay(pdMS_TO_TICKS(100));
            GPIO_ResetBits(SELF_RST_GPIO_PORT, SELF_RST_GPIO_PIN);   // 拉低，恢复空闲
            printf("[SELF_RST] 100ms后PA0拉低，自复位脉冲结束\r\n");
        }

        // PB4(GD_PWRBTIN#)软件消抖：连续PWRBTN_DEBOUNCE_CNT次采到低电平（≈20ms）就判定为
        // 一次有效按下，给PB5(PWRBTN_OUT#)转发一个200ms低脉冲。中途只要采到高电平就重新计数，
        // 触点弹跳产生的窄毛刺凑不满次数，自然被过滤掉。
        if(GPIO_ReadInputDataBit(GD_PWRBTIN_GPIO_PORT, GD_PWRBTIN_GPIO_PIN) == Bit_RESET)
        {
            if(pwrbtn_low_cnt < PWRBTN_DEBOUNCE_CNT)
            {
                pwrbtn_low_cnt++;
            }

            // 按住不放只转发一次，等松开后才重新武装
            if(pwrbtn_low_cnt >= PWRBTN_DEBOUNCE_CNT && pwrbtn_fired == 0)
            {
                pwrbtn_fired = 1;
                printf("[PWRBTN] PB4低电平满20ms，PB5输出200ms低脉冲转发给核心卡\r\n");
                GPIO_ResetBits(PWRBTN_OUT_GPIO_PORT, PWRBTN_OUT_GPIO_PIN);
                vTaskDelay(pdMS_TO_TICKS(PWRBTN_PULSE_MS));
                GPIO_SetBits(PWRBTN_OUT_GPIO_PORT, PWRBTN_OUT_GPIO_PIN);
                printf("[PWRBTN] 200ms后PB5拉高，转发脉冲结束\r\n");
            }
        }
        else
        {
            pwrbtn_low_cnt = 0;
            pwrbtn_fired = 0;
        }

        // GN32_BL_EN/PANEL_EN_GD/GN32_BL_PWM 直接跟随 P3V3SUS_PG(PB6)：PB6为高则三路都输出高，为低则都输出低
        {
            uint8_t p3v3sus_pg = GPIO_ReadInputDataBit(P3V3SUS_PG_GPIO_PORT, P3V3SUS_PG_GPIO_PIN);

            if(p3v3sus_pg == Bit_SET)
            {
                GPIO_SetBits(GN32_BL_EN_GPIO_PORT, GN32_BL_EN_GPIO_PIN);
                GPIO_SetBits(PANEL_EN_GD_GPIO_PORT, PANEL_EN_GD_GPIO_PIN);
                GPIO_SetBits(GN32_BL_PWM_GPIO_PORT, GN32_BL_PWM_GPIO_PIN);
            }
            else
            {
                GPIO_ResetBits(GN32_BL_EN_GPIO_PORT, GN32_BL_EN_GPIO_PIN);
                GPIO_ResetBits(PANEL_EN_GD_GPIO_PORT, PANEL_EN_GD_GPIO_PIN);
                GPIO_ResetBits(GN32_BL_PWM_GPIO_PORT, GN32_BL_PWM_GPIO_PIN);
            }
        }

        // PA15(PWRSUS_EN)/PB13(PWROK)/PB3(PWREN) 直接跟随 PC0(SLP_S3#，开机自检信号)：
        // PC0为高(开机)则三路都输出高，为低(关机)则都输出低
        {
            uint8_t slp_s3 = GPIO_ReadInputDataBit(SLP_S3_GPIO_PORT, SLP_S3_GPIO_PIN);

            if(slp_s3 == Bit_SET)
            {
                GPIO_SetBits(PWRSUS_EN_GPIO_PORT, PWRSUS_EN_GPIO_PIN);
                GPIO_SetBits(PWROK_GPIO_PORT, PWROK_GPIO_PIN);
                GPIO_SetBits(PWREN_GPIO_PORT, PWREN_GPIO_PIN);

                if(pre_pc0_follow != Bit_SET)
                {
                    printf("[PC0_FOLLOW] 已执行拉高：PWRSUS_EN/PWROK/PWREN\r\n");
                }
            }
            else
            {
                GPIO_ResetBits(PWRSUS_EN_GPIO_PORT, PWRSUS_EN_GPIO_PIN);
                GPIO_ResetBits(PWROK_GPIO_PORT, PWROK_GPIO_PIN);
                GPIO_ResetBits(PWREN_GPIO_PORT, PWREN_GPIO_PIN);

                if(pre_pc0_follow != Bit_RESET)
                {
                    printf("[PC0_FOLLOW] 已执行拉低：PWRSUS_EN/PWROK/PWREN\r\n");
                }
            }

            pre_pc0_follow = slp_s3;
        }

        // 各输入信号的电平变化打印（仅用于调试观察，没有其他联动逻辑）
        report_level_change(P3V3SUS_PG_GPIO_PORT, P3V3SUS_PG_GPIO_PIN, &pre_sus_pg,
                             "[P3V3SUS_PG] 变高，PA4(BL_EN)/PA5(PANEL_EN)/PC8(BL_PWM)拉高\r\n",
                             "[P3V3SUS_PG] 变低，PA4(BL_EN)/PA5(PANEL_EN)/PC8(BL_PWM)拉低\r\n");

        report_level_change(P3V3_STBY_PG_GPIO_PORT, P3V3_STBY_PG_GPIO_PIN, &pre_stby_pg,
                             "[P3V3_STBY_PG] 变高\r\n",
                             "[P3V3_STBY_PG] 变低\r\n");

        report_level_change(CB_RESET_GPIO_PORT, CB_RESET_GPIO_PIN, &pre_cb_reset,
                             "[CB_RESET#] 检测到核心卡复位信号变高（复位释放）\r\n",
                             "[CB_RESET#] 检测到核心卡复位信号变低（复位中）\r\n");

        report_level_change(SLP_S3_GPIO_PORT, SLP_S3_GPIO_PIN, &pre_slp_s3,
                             "[SLP_S3#] 变高（开机，PA15(PWRSUS_EN)/PB13(PWROK)/PB3(PWREN)拉高）\r\n",
                             "[SLP_S3#] 变低（关机，PA15(PWRSUS_EN)/PB13(PWROK)/PB3(PWREN)拉低）\r\n");

        report_level_change(SLP_S4_GPIO_PORT, SLP_S4_GPIO_PIN, &pre_slp_s4,
                             "[SLP_S4#] 变高（退出S4休眠）\r\n",
                             "[SLP_S4#] 变低（进入S4休眠）\r\n");

        report_level_change(SLP_S5_GPIO_PORT, SLP_S5_GPIO_PIN, &pre_slp_s5,
                             "[SLP_S5#] 变高（退出S5关机）\r\n",
                             "[SLP_S5#] 变低（进入S5关机）\r\n");
    }
}
