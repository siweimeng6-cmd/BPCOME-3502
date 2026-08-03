# MB-FD2KGD Sheet3 改动 — 测试方案

工程：Keil MDK5 + STM32F103RET6，裸机 FreeRTOS，无单元测试框架，工程文件
`Project/RVMDK（uv5）/BPCOME-3502.uvprojx`。

涉及文件：`User/gpio/bsp_gpio.*`、`User/bsp_adc.*`、`User/timer/bsp_pwm.*`、
`User/func_pwm.c`、`User/task_usart.c`、`User/bsp_i2c.*`（IPMB硬件I2C2）、
`User/bsp_mo_i2c.*` + `User/bsp_temp.c`（模拟I2C测温）、`User/usart/bsp_usart.*`。

测试分两层：**第一层（编译/静态核对，不需要板子）**、**第二层（上板电气/功能测试，需要板子和仪器）**。

通用工具：Keil MDK5、USB转串口(接UART4)、万用表、可调电源/跳线、双通道示波器、
信号发生器（第7节用）、逻辑分析仪或USB转串口模块（第8节用）。

---

## 1. 编译级检查

**步骤**：
1. Keil `Project -> Rebuild all target files`，确认 0 Error，Warning 无新增。
2. 搜索确认这些已删除符号无残留引用：
   `fan1_duty/fan2_duty/fan3_duty/fan4_duty`、`fan1_freq/fan2_freq/fan3_freq/fan4_freq`、
   `BMC_FAN_TACH1~4`、`BMC_FAN_PWM1~4`、`CPLD_GPIO_A4`、`CPLD_GPIO_A5`、`BMC_P12V_FAN_EN`、
   `ADC_CNT5`、`COM_I2C1`、`I2C1_Write`、`I2C1_EV_IRQHandler`、`I2C1_ER_IRQHandler`。
3. 注意 `ADC_CNT4`（PC4，电流监测）**没有被删除**，仍在使用，不要误删或误判为残留引用。

**预期结果**：0 Error，无新增 Warning，符号搜索均为空（`task_gpio.c` 里的历史遗留除外，
该文件未编入构建）。

---

## 2. 静态引脚核对（Sheet3「定义」 vs `bsp_gpio.h`）

**步骤**：对照下表，在 Keil 里搜索对应宏定义所在的 `GPIO_Init()` 调用，核对 `GPIO_Mode` 是否匹配。

| 引脚 | Sheet3 信号 | 方向/极性 | 代码里的处理 | 核对点 |
|---|---|---|---|---|
| PA0 | SELF_RST | out，高电平有效，空闲低 | 主动初始化(`GPIO_Out_PP`)，收到串口`Reset`指令后拉高100ms自复位 | 见第9节串口指令表 |
| PA1 | SW_BLACK+ | in/上拉，暂不使用 | 仅宏定义，不初始化 | — |
| PA2/PA3 | SER_RX1/TX1 | 暂不使用 | 已删除原有主动 GPIO_Init | 确认不再驱动这两个脚 |
| PA4 | GN32_BL_EN | out/下拉，暂不使用 | 仅宏定义，不初始化 | 确认原 `BMC_P12V_FAN_EN` 逻辑已删除 |
| PA6 | CB_RESET# | in/无 | `GPIO_Mode_IN_FLOATING` | 用万用表/示波器验证浮空输入无上下拉 |
| PA7 | ADC1(12V,×12) | ADC | 未改动 | 见第5节 |
| PA8 | FAN_TACH(out) | — | GPIO输出，由EXTI镜像PA9驱动 | 见第7节 |
| PA9 | FAN1_TACH(in) | — | `GPIO_Mode_IN_FLOATING` + EXTI9 | 见第7节 |
| PA13/PA14 | SWDIO/SWCLK | 烧录口 | 未改动（MCU默认功能） | 确认仿真器仍能连接 |
| PB0/PB1 | ADC2/ADC3 | ADC | 未改动，系数维持代码原值 | 见第5节 |
| PB6 | P3V3SUS_PG | in/上拉 | `GPIO_Mode_IPU` | 见第3节 |
| PB7 | P3V3_STBY_PG | in/上拉 | `GPIO_Mode_IPU` | 见第3节 |
| PB8/PB9 | MO_SCL/MO_SDA | I2C(模拟，测温) | `bsp_mo_i2c.c` | 见第6节 |
| PB13 | PWROK | out/下拉 | `GPIO_Mode_Out_PP`，初始拉低 | 见第3节 |
| PC0 | SLP_S3# | in/上拉 | `GPIO_Mode_IPU` | 见第4节 |
| PC1 | SLP_S4# | in/上拉 | `GPIO_Mode_IPU` | 见第4节 |
| PC2 | SLP_S5# | in/上拉 | `GPIO_Mode_IPU` | 见第4节 |
| PC4 | MT9221CT_OUT | ADC4(AIN)，电流监测 | `bsp_adc.c` 换算并打印`I_CURRENT` | 见第5节 |
| PC6 | FAN_PWM(in) | — | `GPIO_Mode_IN_FLOATING` + EXTI6 | 见第7节 |
| PC7 | FAN1_PWM(out) | — | `GPIO_Mode_Out_PP`，由EXTI镜像PC6驱动 | 见第7节 |
| PC10/PC11 | DEBUG_TX/RX | 调试串口 | 未改动(UART4) | 见第9节 |
| PC12/PD2 | SER_RX0/TX0 | 串口，健康上报 | UART5外设 | 见第8节 |

其余 Sheet3 标"暂不使用"的预留脚（PA5/PA10-12/PA15/PB2-5/PB10-12/PB14-15/PC3/PC5/PC8/PC9等）
只需确认 `bsp_gpio.h` 有对应注释/宏定义、`bsp_gpio.c` 没有主动初始化它们即可。

---

## 3. 电源时序：PB6(P3V3SUS_PG) → PB13(PWROK)

**步骤**：
1. 上电复位后，确认 PB13 初始为低。
2. 拉高 PB6 对应网络，PB13 应在一个 `GPIO_Task` 轮询周期内（≤100ms）跟着变高。
3. 拉低 PB6，确认 PB13 同样 ≤100ms 内跟随变低。
4. 反复切换，确认无抖动/误触发（PB6内部上拉，外部悬空时应读高）。

**预期结果**：PB13 电平方向跟随 PB6，延迟不超过约100ms。

---

## 4. 睡眠状态输入：PC0/PC1/PC2 (SLP_S3#/S4#/S5#)

**步骤**：
1. 分别拉高/拉低 PC0、PC1、PC2 对应网络。
2. Keil 在线调试 Watch 窗口看 `GPIOC->IDR` 对应位（或临时加printf），确认
   `GPIO_ReadInputDataBit` 读值随外部电平正确变化。
3. 悬空测试：内部已配置上拉（`GPIO_Mode_IPU`），应读到高电平（低有效信号空闲为高，属正常）。

**预期结果**：三个信号读数与外部施加电平一致，悬空时读高。

---

## 5. ADC 采集回归（PA7/PB0/PB1/PC4，共4路）

当前配置：P12V(×12)、P5V(系数≈4.8)、P3.3V(×3.2)、PC4电流监测
（MT9221CT_OUT，VCC=5V/DC模式，敏感度0.2V/A，效率90%）。

**步骤**：
1. 给12V/5V/3.3V三路加已知基准电压，PC4回路加已知电流。
2. 通过 UART4 观察 `Sensor_Task` 打印的 `V_12V`/`V_5V`/`V_3.3V`/`I_CURRENT`。
3. 与万用表/电流表实测值比对，误差应在合理范围内。

**预期结果**：四路读数与实测值基本一致。

---

## 6. 温度传感器回归（PB8/PB9 模拟I2C）

实现在 `bsp_mo_i2c.c` 的 `Init_MO_I2C()`（初始化）和 `bsp_temp.c`（读数换算+打印），
串口打印格式为 `Addr0x90:xx.x C` / `Addr0x92:...` / `Addr0x94:...`。

**步骤**：
1. 观察 UART4 打印的三路温度读数。
2. 用手捂/吹冷风改变环境温度，确认读数随之合理变化。

**预期结果**：三路温度读数与改动前行为一致。

---

## 7. 风扇 PWM/TACH 直通（PC6→PC7，PA9→PA8）

共用 `EXTI9_5_IRQHandler`（`bsp_pwm.c`），中断里直接电平镜像（`Relay_ProcessEdge()`），
转速换算 `freq = 1e6/period_ticks`，`RPM = freq/2*60`（假设每转2脉冲，`func_pwm.c`/`task_usart.c`）。
这是本轮改动里唯一涉及新中断逻辑的部分，需要重点测。

### 7.1 PWM 转发（PC6→PC7）
用信号发生器在 PC6 注入不同占空比/频率方波（含25kHz、接近0%/100%占空比边界），双通道示波器
同时抓 PC6/PC7，对比占空比误差（应可忽略）和延迟（应为微秒级，无明显固定周期延迟）。

### 7.2 TACH 转发（PA9→PA8）
PA9 注入模拟转速方波（典型2脉冲/转，几十到几百Hz），示波器确认 PA8 忠实跟随 PA9，
同时核对 UART4 打印的 RPM 是否与注入频率换算值一致。

### 7.3 中断负载压力测试
PC6=25kHz PWM + PA9较高频率转速信号同时注入，让 EXTI9_5 打满负载（约50k次/秒），
观察其他任务（如串口打印周期）是否正常，确认没有栈溢出打印（`vApplicationStackOverflowHook`）。

**预期结果**：7.1/7.2 波形忠实跟随、RPM换算准确；7.3 高频负载下系统仍正常调度，无死机/栈溢出打印。

---

## 8. UART5 健康上报口（PC12/PD2）

由 `UART5_Task`（`task_usart.c`，优先级2、栈512字）每 **2 秒** 发送一帧多行文本报文，
经 `UART5_SendHealthReport()` → `Usart_SendString(HEALTH_USARTx, str)` 阻塞式发出。
**只发不收**：UART5 未使能 RXNE 中断、未配 NVIC，核心卡下发的命令收不到。

报文格式（标签统一用ASCII，避免跨设备编码问题）：

```
=========== HEALTH REPORT ===========
V_12V:12.05V
V_5V:5.02V
V_3.3V:3.31V
I_CURRENT:1.23A
Addr0x90:35.5 C
Addr0x92:36.0 C
Addr0x94:34.8 C
FAN_RPM:3600
P3V3SUS_PG:1 P3V3_STBY_PG:1 PWROK:1
CB_RESET#:1 SLP_S3#:1 SLP_S4#:1 SLP_S5#:1
=====================================
```

数据来源：电压/电流读 `stADC_Data`、温度读 `board_temp0/1/2`（配合
`board_temp_valid[3]` 判断有效性）、转速读 `g_fan_tach_relay`、GPIO 现场读取
（PWROK 是输出脚，用 `GPIO_ReadOutputDataBit`）。ADC 和温度由 `Sensor_Task`
每 2 秒刷新，`UART5_Task` 只是消费者，两个任务相位独立，上报数据最多滞后一个采集周期。

**步骤**：
1. USB转串口模块接 PC12（TX）+ GND，115200-8N1，确认每 2 秒收到一帧上述格式报文。
2. 同时开 UART4 窗口交叉核对：UART5 的电压/温度/转速数值应与 UART4 的"系统状态"
   打印一致（可能相差一个采集周期）。
3. 拉高/拉低 PB6(P3V3SUS_PG)，报文里的 `P3V3SUS_PG` 和 `PWROK` 两位应在下一帧跟随变化。
4. **温度掉线测试**：拔掉/断开某一路 LM75A 温度传感器，该路应在下一帧变成
   `Addr0xXX:N/A`，而不是继续发掉线前的旧温度；重新接上后应恢复正常读数。
5. 如需验证接收方向（PD2/RX），可用杜邦线临时短接 PC12-PD2 自环，配合
   `USART_ReceiveData` 验证收发通路都正常（当前固件不解析收到的数据）。

**预期结果**：报文按 2 秒周期稳定输出，各字段与 UART4 打印一致；传感器掉线时对应
温度字段为 `N/A`。

---

## 9. UART4 Debug 口回归 + 串口指令测试

**回归**：观察 `Sensor_Task`（`task_usart.c`）的整体打印格式（版本号/编译时间、
`V_12V`/`V_5V`/`V_3.3V`/`I_CURRENT`、三路温度、风扇RPM等）是否和改动前一致。

**串口指令**：项目目前通过串口接收解析的指令只有一条硬编码指令，没有命令表框架：

| 指令 | 端口 | 格式 | 触发动作 | 源码位置 |
|---|---|---|---|---|
| `Reset` | UART4，115200-8N1 | 单行文本，以`\r`或`\n`结尾，区分大小写，无参数 | 对PA0(SELF_RST)产生一次约100ms**高电平**脉冲触发自复位，空闲态为低 | 匹配逻辑：`bsp_usart.c` `DEBUG_USART_IRQHandler`；执行动作：`bsp_gpio.c` `GPIO_Task` |

**验证步骤**：串口调试助手连 UART4（115200-8N1），发送 `Reset\r\n`，应看到打印
"[SELF_RST] 收到Reset命令，PA0拉高"，约100ms后打印"PA0拉低，自复位脉冲结束"；
有条件的话用示波器/万用表在 PA0 上实测到对应的高电平脉冲。

**预期结果**：Debug口打印内容无变化；`Reset` 指令能被正确识别，PA0脉冲极性、
宽度与代码一致。

---

## 10. 长时间稳定性

项目**未使用 IWDG/WWDG**（无独立/窗口看门狗）。异常时只有 `vApplicationStackOverflowHook`
（仅打印，`NVIC_SystemReset()` 当前被注释掉未启用）和 `HardFault_Handler`（死循环），
都不会主动复位 MCU，测试时不要预期"看门狗复位"会发生。

**步骤**：板子连续上电运行数小时（有条件可叠加第7.3节压力场景），定期观察：
- UART4 打印是否持续正常；
- 是否出现栈溢出打印或 `HardFault_Handler` 死循环（不应出现）；
- PWROK/PWM转发/TACH转发是否长期保持正确跟随，PA0空闲电平是否持续为低。

**预期结果**：数小时运行无异常。
