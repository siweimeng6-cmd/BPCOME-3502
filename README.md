# BPCOME-3502

## 项目信息

- **负责人**：潘工
- **项目周期**：2026 年 7 月至今
- **开发环境**：Keil MDK5（uv5）
- **主控芯片**：STM32F103RET6，裸机 + FreeRTOS
- **需求依据**：[BPCOME-3502单片机需求260723.xlsx](./BPCOME-3502单片机需求260723.xlsx)

## 目录结构

- `User/` — 应用层代码（GPIO、ADC、PWM、I2C、UART、EEPROM 等 BSP 驱动）
- `FreeRTOS/` — FreeRTOS 内核源码与移植层
- `Libraries/` — STM32F10x 标准外设库
- `Project/RVMDK（uv5）/` — Keil 工程文件
- `Doc/` — 需求/测试相关文档

## 相关文档

- [BPCOME-3502单片机需求260723.xlsx](./BPCOME-3502单片机需求260723.xlsx) — Sheet3「定义」为 GPIO/ADC/PWM/I2C/UART 引脚与逻辑定义依据
- [Doc/测试方案_Sheet3改动.md](./Doc/测试方案_Sheet3改动.md) — 针对 Sheet3 改动的编译级 + 上板测试方案
- [Doc/readme.txt](./Doc/readme.txt) — GPIO 寄存器速查笔记

## 变更记录

> 记录内容按时间倒序排列，最新的改动写在最上面。

### 2026-08-04

- 按 Sheet3 需求实现 PA4(GN32_BL_EN,屏背光使能) / PA5(PANEL_EN_GD,屏供电使能)：由"预留未使用"改为推挽输出、初始拉低，逻辑与已有的 PWROK(PB13) 一致——跟随 PB6(P3V3SUS_PG) 电平，PB6高则三路同时输出高。涉及 `User/gpio/bsp_gpio.c`、`User/gpio/bsp_gpio.h`。

### 2026-08-03

- 建立本 Git 仓库，纳入 `User`/`FreeRTOS`/`Libraries`/`Project`/`Doc` 及需求文档，作为后续改动的版本管理起点。
- PA0(SELF_RST) 自复位信号极性由"低电平有效"改为"高电平有效"（空闲拉低，收到串口`Reset`指令后拉高100ms触发），涉及 `User/gpio/bsp_gpio.c`、`User/gpio/bsp_gpio.h`。
- 重写 `Doc/测试方案_Sheet3改动.md`：修正 ADC 实际为4通道（非"5砍到3"）、PC4电流监测消费逻辑、温度采集实现路径（`bsp_mo_i2c.c`/`bsp_temp.c`）、工程文件名等与代码不符的描述，并新增串口指令（`Reset`）说明章节。
- 新增 `UART5_Task` 健康上报任务（优先级2，栈512字）：每2秒通过 UART5(PC12/PD2) 向核心卡发送一帧多行文本报文，内容含3路电压、PC4电流、3路温度、风扇转速及 PG/SLP_Sx/CB_RESET#/PWROK 等 GPIO 状态。涉及 `User/task_usart.c`、`User/task_usart.h`、`User/bsp_init.c`，另在 `User/bsp_adc.h` 导出 `stADC_Data` 供上报读取。
- 温度采集新增有效性标志 `board_temp_valid[3]`（`User/bsp_temp.c`，`User/bsp_mo_i2c.h` 导出）：LM75A 读失败时 `board_tempN` 会保留旧值，此前 UART5 上报无法区分"当前温度"和"传感器掉线后的残留旧值"，现在读失败的那一路上报为 `N/A`。
