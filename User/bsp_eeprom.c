#include "bsp_mo_i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/*
*********************************************************************************************************
*	函 数 名: ee_ReadBytes
*	功能说明: 从串行EEPROM指定地址开始读取若干数据
*	形    参:  _usAddress : 起始地址
*			       _usSize    : 数据长度,单位为字节
*			       _pReadBuf  : 存放读出数据的缓冲区指针
*	返 回 值: 0 表示失败,1表示成功
*********************************************************************************************************
*/
uint8_t ee_ReadBytes(uint8_t *_pReadBuf, uint16_t _usAddress, uint16_t _usSize)
{

	uint16_t i;

	/* 使用串行EEPROM随即读取指定的行，可连续读取几个字节 */

	/* 第1步：发送I2C总线启动信号 */
	i2c_Start();

	/* 第2步：发送控制字节，高7bit是地址，bit0是读写方向位，0表示写，1表示读 */
	i2c_SendByte(EE_DEV_ADDR | I2C_WR);	/* 此处是写指令 */

	/* 第3步：检测ACK */
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应答 */
	}

	/* 第4步：发送字节地址，24C02只有256字节，因此1个字节就够了，如果是24C04以上，那么此处需要发送两个字节 */
	if (EE_ADDR_BYTES == 1)
	{
		i2c_SendByte((uint8_t)_usAddress);
		if (i2c_WaitAck() != 0)
		{
			goto cmd_fail;	/* EEPROM器件无应答 */
		}
	}
	else
	{
		i2c_SendByte(_usAddress >> 8);
		if (i2c_WaitAck() != 0)
		{
			goto cmd_fail;	/* EEPROM器件无应答 */
		}

		i2c_SendByte(_usAddress);
		if (i2c_WaitAck() != 0)
		{
			goto cmd_fail;	/* EEPROM器件无应答 */
		}
	}

	/* 第6步：重新启动I2C总线，下面开始读取数据 */
	i2c_Start();

	/* 第7步：发送控制字节，高7bit是地址，bit0是读写方向位，0表示写，1表示读 */
	i2c_SendByte(EE_DEV_ADDR | I2C_RD);	/* 此处是读指令 */

	/* 第8步：检测ACK */
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应答 */
	}

	/* 第9步：循环读取数据 */
	for (i = 0; i < _usSize; i++)
	{
		_pReadBuf[i] = i2c_ReadByte();	/* 读1个字节 */

		/* 每读完1个字节后都要发送Ack， 最后一个字节不需要Ack，改为Nack */
		if (i != _usSize - 1)
		{
			i2c_Ack();	/* 中间字节都由主CPU产生ACK信号(driver SDA = 0) */
		}
		else
		{
			i2c_NAck();	/* 最后1个字节都由主CPU产生NACK信号(driver SDA = 1) */
		}
	}
	/* 发送I2C总线停止信号 */
	i2c_Stop();
	return 1;	/* 执行成功 */

cmd_fail: /* 命令执行失败后切记发送停止信号，避免影响I2C总线上的其他设备 */
	/* 发送I2C总线停止信号 */
	i2c_Stop();
	return 0;

}

/*
*********************************************************************************************************
*	函 数 名: ee_WriteBytes
*	功能说明: 向串行EEPROM指定地址写入若干数据,采用页写方式提高写数据效率
*	形    参:  _usAddress : 起始地址
*			       _usSize    : 数据长度,单位为字节
*			       _pWriteBuf : 存放待写入数据的缓冲区指针
*	返 回 值: 0 表示失败,1表示成功
*********************************************************************************************************
*/
uint8_t ee_WriteBytes(uint8_t *_pWriteBuf, uint16_t _usAddress, uint16_t _usSize)
{
	uint16_t i,m;
	uint16_t usAddr;

	/*
		写串行EEPROM，可以连续写入很多字节，但是每次写地址只能在同一个page里
		比如24xx02的page size = 8
		简单的处理方法为：单字节写入方式，每写1个字节，都重新发送地址
		为了提高整体的写速效率: 这里采用了page write方式来写
	*/

	usAddr = _usAddress;
	for (i = 0; i < _usSize; i++)
	{
		/* 遇到起始地1个字节或者页面首地址时，需要重新发送启动信号和地址 */
		if ((i == 0) || (usAddr & (EE_PAGE_SIZE - 1)) == 0)
		{
			/*等于，先发送停止信号，允许内部写周期完成*/
			i2c_Stop();

			/* 通过检测器件应答的方式来判断内部写周期是否完成, 一般小于 10ms
				CLK频率为200KHz时，查询次数为30次左右
			*/
			for (m = 0; m < 1000; m++)
			{
				/* 第1步：发送I2C总线启动信号 */
				i2c_Start();

				/* 第2步：发送控制字节，高7bit是地址，bit0是读写方向位，0表示写，1表示读 */
				i2c_SendByte(EE_DEV_ADDR | I2C_WR);	/* 此处是写指令 */

				/* 第3步：发送一个时钟，判断器件是否正确应答 */
				if (i2c_WaitAck() == 0)
				{
					break;
				}
			}
			if (m  == 1000)
			{
				goto cmd_fail;	/* EEPROM器件写超时 */
			}

			/* 第4步：发送字节地址，24C02只有256字节，因此1个字节就够了，如果是24C04以上，那么此处需要发送两个字节 */
			if (EE_ADDR_BYTES == 1)
			{
				i2c_SendByte((uint8_t)usAddr);
				if (i2c_WaitAck() != 0)
				{
					goto cmd_fail;	/* EEPROM器件无应答 */
				}
			}
			else
			{
				i2c_SendByte(usAddr >> 8);
				if (i2c_WaitAck() != 0)
				{
					goto cmd_fail;	/* EEPROM器件无应答 */
				}

				i2c_SendByte(usAddr);
				if (i2c_WaitAck() != 0)
				{
					goto cmd_fail;	/* EEPROM器件无应答 */
				}
			}
		}

		/* 第6步：开始写入数据 */
		i2c_SendByte(_pWriteBuf[i]);

		/* 第7步：检测ACK */
		if (i2c_WaitAck() != 0)
		{
			goto cmd_fail;	/* EEPROM器件无应答 */
		}

		usAddr++;	/* 地址加1 */
	}

	/* 数据执行成功，发送I2C总线停止信号 */
	i2c_Stop();
	return 1;

cmd_fail: /* 命令执行失败后切记发送停止信号，避免影响I2C总线上的其他设备 */
	/* 发送I2C总线停止信号 */
	i2c_Stop();
	return 0;
}

static void ee_Delay(__IO uint32_t nCount)	 //简单的延时函数
{
	for(; nCount != 0; nCount--);
}
/*
*********************************************************************************************************
*	函 数 名: eeprom_test
*	功能说明:  EEPROM读写测试
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
uint8_t eeprom_test(void)
{
	uint16_t i;
	uint8_t write_buf[EE_PAGE_SIZE];
	uint8_t read_buf[EE_PAGE_SIZE];

	/*-----------------------------------------------------------------------------------*/
	if (ee_CheckOk() == 0)
	{
		/* 没有检测到EEPROM */
		printf("没有检测到串行EEPROM!\r\n");

		return 0;
	}
	/*------------------------------------------------------------------------------------*/
	/* 填充测试缓冲区 */
	for (i = 0; i < EE_PAGE_SIZE; i++)
	{
		write_buf[i] = i;
	}
	/*------------------------------------------------------------------------------------*/
	if (ee_WriteBytes(write_buf, 0, EE_PAGE_SIZE) == 0)
	{
		printf("写eeprom出错！\r\n");
		return 0;
	}
	else
	{
		printf("写eeprom成功！\r\n");
	}

	/*写完之后需要适当的延时再去读，不然会出错*/
	ee_Delay(0x0FFFFF);
	/*-----------------------------------------------------------------------------------*/
	if (ee_ReadBytes(read_buf, 0, EE_PAGE_SIZE) == 0)
	{
		printf("读eeprom出错！\r\n");
		return 0;
	}
	else
	{
		printf("读eeprom成功，数据如下：\r\n");
	}
	/*-----------------------------------------------------------------------------------*/
	for (i = 0; i < EE_PAGE_SIZE; i++)
	{
		if(read_buf[i] != write_buf[i])
		{
			printf("0x%02X ", read_buf[i]);
			printf("错误:EEPROM读出与写入的数据不一致\r\n");
			return 0;
		}
		printf(" %02X", read_buf[i]);

		if ((i & 15) == 15)
		{
			printf("\r\n");
		}
	}
	printf("eeprom读写测试成功\r\n");
	return 1;
}

uint32_t g_runtime_total_minutes = 0;
uint32_t g_runtime_countdown_minutes = RUNTIME_SAVE_INTERVAL_MIN;
uint8_t g_runtime_history_valid = 0;

/* V1磁盘格式：5个小端uint32_t，CRC覆盖前4个字，两个槽各占独立页。
 * 旧地址128只读，不会因迁移或新格式写入中断而被覆盖。
 */
#define RUNTIME_RECORD_MAGIC       0x52544D31UL
#define RUNTIME_RECORD_WORDS       5
#define RUNTIME_SAVE_SECONDS       (RUNTIME_SAVE_INTERVAL_MIN * 60UL)
#define RUNTIME_READY_POLLS        1000
#define RUNTIME_NO_SLOT            2

static TickType_t s_runtime_last_tick;
static uint32_t s_runtime_fraction_ticks;
static uint64_t s_runtime_session_seconds;
static uint64_t s_runtime_base_seconds;
static uint64_t s_runtime_saved_seconds;
static uint64_t s_runtime_save_deadline;
static uint64_t s_runtime_next_attempt;
static uint32_t s_runtime_sequence;
static uint8_t s_runtime_active_slot;
static uint8_t s_runtime_valid_mask;
static uint8_t s_runtime_history_ready;
static uint8_t s_runtime_clear_failed;

static uint16_t Runtime_SlotAddress(uint8_t slot)
{
    return slot == 0 ? RUNTIME_EE_SLOT_A_ADDR : RUNTIME_EE_SLOT_B_ADDR;
}

/* CRC-32/ISO-HDLC，固定按小端字节顺序计算，不依赖结构体填充。 */
static uint32_t Runtime_RecordCRC(const uint32_t *record)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t word;
    uint8_t i, byte, bit;

    for (i = 0; i < RUNTIME_RECORD_WORDS - 1; ++i)
    {
        word = record[i];
        for (byte = 0; byte < 4; ++byte)
        {
            crc ^= word & 0xFF;
            word >>= 8;
            for (bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320UL : 0);
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

static uint8_t Runtime_RecordValid(const uint32_t *record)
{
    return record[0] == RUNTIME_RECORD_MAGIC && record[3] < 60 &&
           record[4] == Runtime_RecordCRC(record);
}

static uint8_t Runtime_RecordBlank(const uint32_t *record)
{
    uint8_t i;
    for (i = 0; i < RUNTIME_RECORD_WORDS; ++i)
        if (record[i] != 0xFFFFFFFFUL)
            return 0;
    return 1;
}

/* 任一槽通信失败都不继续写：无法确定未读出的槽是否保存着更新的历史。 */
static uint8_t Runtime_LoadHistory(void)
{
    uint32_t records[2][RUNTIME_RECORD_WORDS];
    uint32_t legacy_minutes;
    uint32_t difference;
    uint8_t valid_a, valid_b, active;

    if (!ee_CheckOk() ||
        !ee_ReadBytes((uint8_t *)records[0], RUNTIME_EE_SLOT_A_ADDR, sizeof(records[0])) ||
        !ee_ReadBytes((uint8_t *)records[1], RUNTIME_EE_SLOT_B_ADDR, sizeof(records[1])))
        return 0;

    valid_a = Runtime_RecordValid(records[0]);
    valid_b = Runtime_RecordValid(records[1]);
    if (valid_a || valid_b)
    {
        active = valid_a ? 0 : 1;
        if (valid_a && valid_b)
        {
            difference = records[1][1] - records[0][1];
            /* 正常双槽序号相邻；相同序号却内容不同或半范围歧义时保护现场。 */
            if (difference == 0x80000000UL ||
                (difference == 0 && memcmp(records[0], records[1], sizeof(records[0])) != 0))
                return 0;
            if (difference != 0 && difference < 0x80000000UL)
                active = 1;
        }
        s_runtime_base_seconds = (uint64_t)records[active][2] * 60 + records[active][3];
        s_runtime_sequence = records[active][1];
        s_runtime_active_slot = active;
        s_runtime_valid_mask = (uint8_t)(valid_a | (valid_b << 1));
    }
    else
    {
        /* 有非空但损坏的新记录时，禁止回退到过时的旧版值并覆盖历史。 */
        if (!Runtime_RecordBlank(records[0]) || !Runtime_RecordBlank(records[1]) ||
            !ee_ReadBytes((uint8_t *)&legacy_minutes, RUNTIME_EE_ADDR, sizeof(legacy_minutes)))
            return 0;
        if (legacy_minutes == 0xFFFFFFFFUL)
            legacy_minutes = 0;
        s_runtime_base_seconds = (uint64_t)legacy_minutes * 60;
        s_runtime_sequence = 0;
        s_runtime_active_slot = RUNTIME_NO_SLOT;
        s_runtime_valid_mask = 0;
    }
    /* 保存目标对齐累计时长的整30分钟；不随上次保存的轮询延迟后移。 */
    s_runtime_save_deadline = (s_runtime_base_seconds / RUNTIME_SAVE_SECONDS + 1) * RUNTIME_SAVE_SECONDS;
    s_runtime_history_ready = 1;
    return 1;
}

static void Runtime_Publish(void)
{
    uint64_t total_seconds = s_runtime_base_seconds + s_runtime_session_seconds;
    uint64_t minutes = total_seconds / 60;
    uint32_t total_minutes = 0;
    uint32_t countdown_minutes = 0;

    if (s_runtime_history_ready)
    {
        total_minutes = minutes > 0xFFFFFFFFUL ? 0xFFFFFFFFUL : (uint32_t)minutes;
        if (total_seconds < s_runtime_save_deadline && s_runtime_valid_mask == 3)
            countdown_minutes = (uint32_t)((s_runtime_save_deadline - total_seconds + 59) / 60);
    }
    /* UART5可能抢占Sensor_Task；三项作为一个快照发布，临界区内不做I2C/打印。 */
    taskENTER_CRITICAL();
    g_runtime_total_minutes = total_minutes;
    g_runtime_countdown_minutes = countdown_minutes;
    g_runtime_history_valid = s_runtime_history_ready;
    taskEXIT_CRITICAL();
}

void Runtime_GetSnapshot(uint32_t *minutes, uint32_t *countdown, uint8_t *valid)
{
    taskENTER_CRITICAL();
    *minutes = g_runtime_total_minutes;
    *countdown = g_runtime_countdown_minutes;
    *valid = g_runtime_history_valid;
    taskEXIT_CRITICAL();
}

/* 最多一页，写入后等待内部写周期完成并逐字节回读确认。 */
static uint8_t Runtime_WriteVerified(uint16_t address, uint8_t *data, uint16_t size)
{
    uint8_t verify[EE_PAGE_SIZE];
    uint16_t poll;

    if (size > sizeof(verify) || !ee_WriteBytes(data, address, size))
        return 0;
    for (poll = 0; poll < RUNTIME_READY_POLLS; ++poll)
        if (ee_CheckOk())
            break;
    return poll < RUNTIME_READY_POLLS &&
           ee_ReadBytes(verify, address, size) && memcmp(data, verify, size) == 0;
}

/* 写非当前槽，等待内部写周期结束，再回读比对。全程保留上一份有效记录。 */
static uint8_t Runtime_Save(void)
{
    uint32_t record[RUNTIME_RECORD_WORDS];
    uint64_t total_seconds = s_runtime_base_seconds + s_runtime_session_seconds;
    uint8_t target = s_runtime_active_slot == 0 ? 1 : 0;

    if (total_seconds / 60 > 0xFFFFFFFFUL)
        return 0; /* 超过对外分钟数表示范围时不回绕覆盖历史。 */
    record[0] = RUNTIME_RECORD_MAGIC;
    record[1] = s_runtime_sequence + 1;
    record[2] = (uint32_t)(total_seconds / 60);
    record[3] = (uint32_t)(total_seconds % 60);
    record[4] = Runtime_RecordCRC(record);
    if (!Runtime_WriteVerified(Runtime_SlotAddress(target), (uint8_t *)record, sizeof(record)))
        return 0;

    s_runtime_sequence = record[1];
    s_runtime_active_slot = target;
    s_runtime_valid_mask |= (uint8_t)(1U << target);
    s_runtime_saved_seconds = s_runtime_session_seconds;
    s_runtime_save_deadline = (total_seconds / RUNTIME_SAVE_SECONDS + 1) * RUNTIME_SAVE_SECONDS;
    printf("[RUNTIME] 累计运行时间 %u小时%u分钟 已写入EEPROM并回读确认（备份%c）\r\n",
           record[2] / 60, record[2] % 60, target == 0 ? 'A' : 'B');
    return 1;
}

/* BSP阶段调用一次。系统启动调度前tick为0；不计上电初始化/断电期间时间。 */
void Runtime_Init(void)
{
    s_runtime_last_tick = xTaskGetTickCount();
    s_runtime_fraction_ticks = 0;
    s_runtime_session_seconds = 0;
    s_runtime_base_seconds = 0;
    s_runtime_saved_seconds = 0;
    s_runtime_save_deadline = 0;
    s_runtime_next_attempt = 0;
    s_runtime_sequence = 0;
    s_runtime_active_slot = RUNTIME_NO_SLOT;
    s_runtime_valid_mask = 0;
    s_runtime_history_ready = 0;
    s_runtime_clear_failed = 0;
    g_runtime_history_valid = 0;

    if (!Runtime_LoadHistory())
    {
        s_runtime_next_attempt = RUNTIME_RETRY_SECONDS;
        printf("[RUNTIME] 历史记录读取失败或损坏，暂停写入并重试\r\n");
    }
    Runtime_Publish();
    if (g_runtime_history_valid)
        printf("[RUNTIME] 单片机累计运行时间: %u小时%u分钟\r\n",
               g_runtime_total_minutes / 60, g_runtime_total_minutes % 60);
}

/* 只允许Sensor_Task调用，与温度采集/自动保存串行。中断保持开启。
 * 擦除成功后重建零时长双备份；任一步失败均返回0并暂停自动保存，
 * 防止把旧RAM历史重新写入已部分擦除的EEPROM，需重新发送ClearRuntime。
 */
uint8_t Runtime_ClearAll(void)
{
    uint8_t erased_page[EE_PAGE_SIZE];
    uint32_t address; /* 64KB终点不能用uint16_t，避免回绕后无限擦除。 */

    s_runtime_clear_failed = 1;
    s_runtime_history_ready = 0;
    Runtime_Publish();
    memset(erased_page, 0xFF, sizeof(erased_page));
    for (address = 0; address < EE_SIZE; address += EE_PAGE_SIZE)
        if (!Runtime_WriteVerified((uint16_t)address, erased_page, sizeof(erased_page)))
            return 0;

    s_runtime_last_tick = xTaskGetTickCount();
    s_runtime_fraction_ticks = 0;
    s_runtime_session_seconds = 0;
    s_runtime_base_seconds = 0;
    s_runtime_saved_seconds = 0;
    s_runtime_save_deadline = RUNTIME_SAVE_SECONDS;
    s_runtime_next_attempt = 0;
    s_runtime_sequence = 0;
    s_runtime_active_slot = RUNTIME_NO_SLOT;
    s_runtime_valid_mask = 0;
    s_runtime_history_ready = 1;
    if (!Runtime_Save() || !Runtime_Save())
    {
        s_runtime_history_ready = 0;
        Runtime_Publish();
        return 0;
    }
    /* 以清除完成时刻作为新计时起点，擦除及建立备份耗时不带入新计时。 */
    s_runtime_last_tick = xTaskGetTickCount();
    s_runtime_clear_failed = 0;
    Runtime_Publish();
    return 1;
}

/* 只由Sensor_Task调用，保持与温度采集的模拟I2C串行访问。
 * 两次调用间隔必须小于一个tick回绕周期（当前1kHz/32位约49.7天）。
 */
void Runtime_Task_Update(void)
{
    TickType_t now = xTaskGetTickCount();
    TickType_t delta = (TickType_t)(now - s_runtime_last_tick);
    uint64_t ticks = (uint64_t)delta + s_runtime_fraction_ticks;

    s_runtime_last_tick = now;
    s_runtime_session_seconds += ticks / configTICK_RATE_HZ;
    s_runtime_fraction_ticks = (uint32_t)(ticks % configTICK_RATE_HZ);

    if (s_runtime_clear_failed)
    {
        Runtime_Publish();
        return;
    }

    if (s_runtime_session_seconds >= s_runtime_next_attempt)
    {
        if (!s_runtime_history_ready && !Runtime_LoadHistory())
        {
            s_runtime_next_attempt = s_runtime_session_seconds + RUNTIME_RETRY_SECONDS;
        }
        else if (s_runtime_valid_mask != 3 ||
                 s_runtime_base_seconds + s_runtime_session_seconds >= s_runtime_save_deadline)
        {
            if (Runtime_Save())
            {
                /* 首次迁移/单槽恢复时尽快补齐另一份备份。 */
                s_runtime_next_attempt = s_runtime_session_seconds;
            }
            else
            {
                s_runtime_next_attempt = s_runtime_session_seconds + RUNTIME_RETRY_SECONDS;
                printf("[RUNTIME] EEPROM保存未通过确认，保留旧记录，稍后重试\r\n");
            }
        }
    }
    Runtime_Publish();
}
