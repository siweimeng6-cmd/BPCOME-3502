# MB-FD2KGD Sheet3 改动 — 测试方案

本方案针对按 `BPCOME-3502单片机需求260723.xlsx` Sheet3「定义」重构后的 GPIO/ADC/PWM/I2C/UART 代码
（`User/gpio/bsp_gpio.*`、`User/bsp_adc.*`、`User/timer/bsp_pwm.*`、`User/func_pwm.c`、
`User/task_usart.c`、`User/bsp_i2c.*`、`User/usart/bsp_usart.*`）。

工程是 Keil MDK5 + STM32F103RET6 裸机 FreeRTOS，没有单元测试框架，测试分两层：
**第一层（编译/静态核对，不需要板子）**、**第二层（上板电气/功能测试，需要板子和仪器）**。
每节包含：测试目的 / 前置条件 / 步骤 / 预期结果 / 所需工具。

---

## 1. 编译级检查

**目的**：确认改动后工程能正常全量编译，没有对已删除符号的悬空引用。

**前置条件**：Keil MDK5 环境，工程路径 `Project/RVMDK（uv5）/MB-FD2KGD.uvprojx`。

**步骤**：
1. 用 Keil 打开工程，`Project -> Rebuild all target files`。
2. 检查 Build Output：0 Error。
3. 检查 Warning 数量相比改动前有没有新增（尤其是隐式声明、未使用变量之外的新增警告）。
4. 重点搜索这些已删除符号，确认没有残留引用（编译器会直接报错，但先手动搜一遍能更快定位）：
   `fan1_duty/fan2_duty/fan3_duty/fan4_duty`、`fan1_freq/fan2_freq/fan3_freq/fan4_freq`、
   `BMC_FAN_TACH1~4`、`BMC_FAN_PWM1~4`、`CPLD_GPIO_A4`、`CPLD_GPIO_A5`、`BMC_P12V_FAN_EN`、
   `ADC_CNT4`、`ADC_CNT5`、`COM_I2C1`、`I2C1_Write`、`I2C1_EV_IRQHandler`、`I2C1_ER_IRQHandler`。

**预期结果**：0 Error，无新增 Warning，上述符号搜索结果全部为空（`task_gpio.c` 里的历史遗留除外，
该文件未编入构建，不影响）。

**所需工具**：Keil MDK5。

---

## 2. 静态引脚核对（Sheet3「定义」 vs `bsp_gpio.h`）

**目的**：逐项确认每个物理引脚的方向、上下拉在代码里配置正确。

**步骤**：对照下表，在 Keil 里搜索对应宏定义所在的 `GPIO_Init()` 调用，核对 `GPIO_Mode` 是否匹配。

| 引脚 | Sheet3 信号 | 方向/上下拉 | 代码里的处理 | 核对点 |
|---|---|---|---|---|
| PA0 | SELF_RST | out/上拉 | 仅定义宏，未主动 GPIO_Init（无触发逻辑可依据） | 确认未被其他地方误用 |
| PA1 | SW_BLACK+ | in/上拉，暂不使用 | 仅宏定义，不初始化 | — |
| PA2/PA3 | SER_RX1/TX1 | 暂不使用 | 已删除原有主动 GPIO_Init | 确认不再驱动这两个脚 |
| PA4 | GN32_BL_EN | out/下拉，暂不使用 | 仅宏定义，不初始化 | 确认原 `BMC_P12V_FAN_EN` 逻辑已删除 |
| PA6 | CB_RESET# | in/无 | `GPIO_Mode_IN_FLOATING`，`bsp_gpio.c` | 用万用表/示波器验证浮空输入无上下拉 |
| PA7 | ADC1(12V,×12) | ADC | 未改动 | 见第5节 |
| PA8 | FAN_TACH(out) | — | GPIO输出，由EXTI镜像PA9驱动 | 见第7节 |
| PA9 | FAN1_TACH(in) | — | `GPIO_Mode_IN_FLOATING` + EXTI9 | 见第7节 |
| PA13/PA14 | SWDIO/SWCLK | 烧录口 | 未改动（MCU默认功能） | 确认仿真器仍能连接 |
| PB0/PB1 | ADC2/ADC3 | ADC | 未改动，系数维持代码原值 | 见第5节 |
| PB6 | P3V3SUS_PG | in/上拉 | `GPIO_Mode_IPU` | 见第3节 |
| PB7 | P3V3_STBY_PG | in/上拉 | `GPIO_Mode_IPU` | 见第3节 |
| PB8/PB9 | MO_SCL/MO_SDA | I2C(模拟) | 未改动 | 见第6节 |
| PB13 | PWROK | out/下拉 | `GPIO_Mode_Out_PP`，初始拉低 | 见第3节 |
| PC0 | SLP_S3# | in/上拉 | `GPIO_Mode_IPU` | 见第4节 |
| PC1 | SLP_S4# | in/上拉 | `GPIO_Mode_IPU` | 见第4节 |
| PC2 | SLP_S5# | in/上拉 | `GPIO_Mode_IPU` | 见第4节 |
| PC4 | MT9221CT_OUT | in/无 | `GPIO_Mode_IN_FLOATING` | 仅配置，无消费逻辑 |
| PC6 | FAN_PWM(in) | — | `GPIO_Mode_IN_FLOATING` + EXTI6 | 见第7节 |
| PC7 | FAN1_PWM(out) | — | `GPIO_Mode_Out_PP`，由EXTI镜像PC6驱动 | 见第7节 |
| PC10/PC11 | DEBUG_TX/RX | 调试串口 | 未改动(UART4) | 见第9节 |
| PC12/PD2 | SER_RX0/TX0 | 串口，健康上报 | 新增UART5外设 | 见第8节 |

其余 Sheet3 中标"暂不使用"的预留脚（PA5/PA10-12/PA15/PB2-5/PB10-12/PB14-15/PC3/PC5/PC8/PC9等）
只需确认 `bsp_gpio.h` 里有对应注释/宏定义、且 `bsp_gpio.c` 没有主动初始化它们。

**所需工具**：Keil（源码查看）、万用表（可选，验证悬空/上拉电平）。

---

## 3. 电源时序：PB6(P3V3SUS_PG) → PB13(PWROK)

**目的**：验证 PWROK 输出正确跟随 P3V3SUS_PG 输入。

**前置条件**：板子上电，能用可调电源或跳线模拟 PB6 对应网络的高低电平；示波器或万用表接 PB13。

**步骤**：
1. 上电复位后，先确认 PB13 初始为低（`bsp_gpio_init()` 里已强制拉低）。
2. 将 P3V3SUS_PG 对应网络拉高（模拟电源好），用示波器/万用表观察 PB13 是否在一个 `GPIO_Task`
   轮询周期内（≤100ms）跟着变高。
3. 再将其拉低，确认 PB13 同样在 ≤100ms 内跟随变低。
4. 反复切换几次，确认没有抖动/误触发（PB6 内部已配置上拉，外部悬空时应读为高）。

**预期结果**：PB13 电平与 PB6 电平方向一致，延迟不超过约100ms（轮询周期）。

**所需工具**：可调电源/跳线、万用表或示波器。

---

## 4. 睡眠状态输入：PC0/PC1/PC2 (SLP_S3#/S4#/S5#)

**目的**：确认三个核心卡睡眠状态信号被正确读取为数字输入。

**步骤**：
1. 分别拉高/拉低 PC0、PC1、PC2 对应网络。
2. 用调试器（Keil 在线调试，Watch 窗口看 `GPIOC->IDR` 对应位，或临时加一行 printf）确认
   `GPIO_ReadInputDataBit` 读到的值随外部电平正确变化。
3. 悬空测试：外部不接时，因为内部配置了上拉（`GPIO_Mode_IPU`），应读到高电平（对应"未睡眠/未关机"，
   低有效信号空闲时为高属正常）。

**预期结果**：三个信号读数与外部施加电平一致，悬空时读高。

**所需工具**：Keil 在线调试器、跳线/万用表。

---

## 5. ADC 采集回归（PA7/PB0/PB1，12V/5V/3.3V，未改动部分）

**目的**：确认本轮改动（主要是把 ADC 通道数从5砍到3）没有影响剩下3路的采集精度。

**步骤**：
1. 给 12V/5V/3.3V 三路对应的分压点加已知的基准电压。
2. 通过 debug 串口（UART4）观察 `Sensor_Task` 打印的 `V_12V`/`V_5V`/`V_3.3V`。
3. 与万用表实测值比对，误差应在个位百分比以内（原有分压电阻误差范围内，非本轮改动引入的误差）。

**预期结果**：三路电压读数与实测值基本一致，且此前若有的误差特征应保持不变（说明只是通道数变了，
测量链路本身没受影响）。

**所需工具**：可调直流电源、万用表、USB转串口(接UART4)。

---

## 6. 温度传感器回归（PB8/PB9 模拟I2C）

**目的**：确认公共部分改动（GPIO时钟使能等）没有误伤原本正常工作的温度采集。

**步骤**：
1. 观察 debug 串口打印的三路温度读数（`Board_ADDR90/92/94_temp`）。
2. 用手捂/吹冷风等方式改变环境温度，确认读数随之合理变化。

**预期结果**：三路温度读数与改动前行为一致。

**所需工具**：USB转串口。

---

## 7. 风扇 PWM/TACH 直通（PC6→PC7，PA9→PA8）

这是本轮改动里唯一涉及新中断逻辑（EXTI9_5）的部分，需要重点测。

### 7.1 PWM 转发（PC6 输入 → PC7 输出）
**步骤**：
1. 用信号发生器/PWM源在 PC6 注入不同占空比、不同频率的方波（覆盖典型风扇PWM频率，如25kHz，
   以及边界情况如接近0%/100%占空比）。
2. 双通道示波器同时抓 PC6（输入）和 PC7（输出），对比：
   - 占空比误差（预期：几乎一致，因为是电平实时镜像，不经过量化）
   - 延迟（预期：微秒级，即 EXTI 中断响应延迟，不应有明显的固定周期性延迟）

**预期结果**：PC7 波形忠实跟随 PC6，占空比误差可忽略，延迟在微秒级。

### 7.2 TACH 转发（PA9 输入 → PA8 输出）
**步骤**：
1. 用信号发生器在 PA9 注入模拟风扇转速方波（典型两脉冲/转，几十到几百Hz）。
2. 示波器抓 PA9 和 PA8，确认 PA8 忠实跟随 PA9。
3. 同时看 debug 串口打印的风扇转速（RPM），核对换算是否合理（`freq = 1e6/period_ticks`，
   `RPM = freq/2*60`，注意目前代码假设每转2个脉冲）。

**预期结果**：PA8 跟随 PA9，串口打印的 RPM 与注入频率换算值一致。

### 7.3 中断负载压力测试
**步骤**：
1. 同时在 PC6 注入 25kHz PWM、PA9 注入较高频率的模拟转速信号，让 EXTI9_5 中断打满负载
   （PC6在25kHz下每边沿触发一次中断，相当于50k次/秒）。
2. 观察系统其他任务是否正常运行（debug 串口打印是否还按预期周期输出、有没有卡顿或丢失）。
3. 检查 FreeRTOS 栈水位（如果工程里有 `uxTaskGetStackHighWaterMark` 或类似监控手段）确认没有
   栈溢出迹象，`vApplicationStackOverflowHook` 没有被触发。

**预期结果**：高频中断负载下系统仍正常调度，无死机、无栈溢出。

**所需工具**：信号发生器/函数发生器（双通道更佳）、双通道示波器、USB转串口。

---

## 8. UART5 健康上报口（PC12/PD2）

**目的**：具体上报协议还未定义，本节只验证外设本身工作正常。

**步骤**：
1. 用逻辑分析仪或串口转USB模块接到 PC12（TX），在代码里临时调用一次
   `UART5_SendHealthReport("test\r\n")`（或在 `Sensor_Task` 里加一行临时调用）。
2. 确认能在 PC12 上抓到波特率 115200、8N1 格式正确的串口数据。
3. 如果需要验证接收方向（PD2/RX），可用杜邦线临时短接 PC12-PD2 做自环测试，配合
   `USART_ReceiveData` 读取验证收发通路都正常。

**预期结果**：UART5 外设能正常发送/接收数据，波特率、帧格式正确。此测试通过后，
后续补充具体健康上报协议内容时无需再验证外设层。

**所需工具**：逻辑分析仪或USB转串口模块。

---

## 9. Debug 串口 (UART4) 总体回归

**目的**：确认本轮改动没有影响一直在用的 debug 串口。

**步骤**：观察 `Sensor_Task` 输出格式、`BSP_Init` 里的版本号/初始化打印是否和改动前一致。

**预期结果**：无变化。

**所需工具**：USB转串口。

---

## 10. 长时间稳定性

**目的**：排查新增中断（EXTI9_5）、新的电源时序轮询逻辑是否有长期运行下才暴露的问题。

**步骤**：让板子连续上电运行数小时（有条件的话叠加第7节的中断压力场景），定期观察：
- debug 串口打印是否持续正常
- 是否出现看门狗复位、`HardFault_Handler` 死循环、`vApplicationStackOverflowHook` 打印
- PWROK/PWM转发/TACH转发是否长期保持正确跟随，没有随时间漂移或失步

**预期结果**：数小时运行无异常。

**所需工具**：USB转串口、示波器（抽查）、耐心。
