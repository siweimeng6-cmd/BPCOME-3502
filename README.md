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

### 2026-08-03

- 建立本 Git 仓库，纳入 `User`/`FreeRTOS`/`Libraries`/`Project`/`Doc` 及需求文档，作为后续改动的版本管理起点。
