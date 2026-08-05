#include "bsp_mo_i2c.h"

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

static uint32_t s_base_minutes = 0;        // 开机时从EEPROM读到的历史累计分钟数
static uint32_t s_elapsed_seconds = 0;     // 本次开机已运行秒数(Sensor_Task每次+2)
static uint32_t s_last_saved_minutes = 0;  // 本次开机上一次写EEPROM时的已运行分钟数

/*
*********************************************************************************************************
*	函 数 名: Runtime_Init
*	功能说明: 开机时调用一次，从EEPROM读取历史累计运行时间(分钟)，作为本次计时的起点
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void Runtime_Init(void)
{
	uint32_t stored = 0;

	if (ee_CheckOk() && ee_ReadBytes((uint8_t *)&stored, RUNTIME_EE_ADDR, sizeof(stored)))
	{
		if (stored == 0xFFFFFFFF)   /* EEPROM擦除态，视为第一次开机 */
		{
			stored = 0;
		}
	}
	else
	{
		stored = 0;
		printf("[RUNTIME] 读取EEPROM累计运行时间失败，按0开始计时\r\n");
	}

	s_base_minutes = stored;
	g_runtime_total_minutes = stored;
	g_runtime_countdown_minutes = RUNTIME_SAVE_INTERVAL_MIN;

	printf("[RUNTIME] 单片机累计运行时间: %u小时%u分钟\r\n", g_runtime_total_minutes / 60, g_runtime_total_minutes % 60);
}

/*
*********************************************************************************************************
*	函 数 名: Runtime_Task_Update
*	功能说明: 周期性调用(与调用者的轮询周期一致，当前为Sensor_Task每2秒调用一次)，
*	         累加本次开机运行时长；每满RUNTIME_SAVE_INTERVAL_MIN分钟，把累计总时长写入EEPROM一次
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void Runtime_Task_Update(void)
{
	uint32_t elapsed_minutes;

	s_elapsed_seconds += 2;
	elapsed_minutes = s_elapsed_seconds / 60;

	g_runtime_total_minutes = s_base_minutes + elapsed_minutes;

	if (elapsed_minutes - s_last_saved_minutes >= RUNTIME_SAVE_INTERVAL_MIN)
	{
		s_last_saved_minutes = elapsed_minutes;

		if (ee_WriteBytes((uint8_t *)&g_runtime_total_minutes, RUNTIME_EE_ADDR, sizeof(g_runtime_total_minutes)))
			printf("[RUNTIME] 累计运行时间 %u小时%u分钟 已写入EEPROM\r\n", g_runtime_total_minutes / 60, g_runtime_total_minutes % 60);
		else
			printf("[RUNTIME] 写入EEPROM失败\r\n");
	}

	g_runtime_countdown_minutes = RUNTIME_SAVE_INTERVAL_MIN - (elapsed_minutes - s_last_saved_minutes);
}
