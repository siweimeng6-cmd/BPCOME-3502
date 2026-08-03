#include "bsp_init.h"
#include "bsp_mo_i2c.h"


int board_temp0[2]={0x00,0x00};           //存储整数化后的实际温度值
int board_temp1[2]={0x00,0x00};           //存储整数化后的实际温度值
int board_temp2[2]={0x00,0x00};           //存储整数化后的实际温度值

// 三路温度本次读取是否成功，下标0/1/2对应board_temp0/1/2。
// 读失败时board_tempN保留上一次的旧值，靠这个标志区分"当前有效"和"传感器掉线"。
int board_temp_valid[3]={0x00,0x00,0x00};

uint8_t board_lm75a_temp0[2]={0x00,0x00}; //温度采集数字化存储寄存器
uint8_t board_lm75a_temp1[2]={0x00,0x00}; //温度采集数字化存储寄存器
uint8_t board_lm75a_temp2[2]={0x00,0x00}; //温度采集数字化存储寄存器


/*
*********************************************************************************************************
*	函 数 名: Board_ADDR90_temp
*	功能说明: 监测地址为0x90的温度芯片
*	形    参: 
*********************************************************************************************************
*/
void Board_ADDR90_temp(void)
{
		float float_temp;
		char flash_buf[128] = {0};
		if(board_lm75a_temp_ReadBytes((uint8_t *)board_lm75a_temp0,0,2,BOARD_LM75A90_ADDRESS)==1)
		{
			float_temp = temp_calculate(board_lm75a_temp0); //采集转换成实际温度
			decimal_2_integer(float_temp,board_temp0); //整数化
			board_temp_valid[0] = 1;
			sprintf((char *)flash_buf,"Addr0x90:%d.%d C\r\n" ,board_temp0[0],board_temp0[1]);
		}
		else
		{
			board_temp_valid[0] = 0;
			sprintf((char *)flash_buf,"Addr0x90 TEMP read failed.\r\n");
		}
		strcat((char *)stPrintf_Buf.buf, flash_buf);
}
/*
*********************************************************************************************************
*	函 数 名: Board_ADDR92_temp
*	功能说明: 监测地址为0x92的温度芯片
*	形    参:  
*********************************************************************************************************
*/
void Board_ADDR92_temp(void)
{		
		float float_temp;
		char flash_buf[128] = {0};
		if(board_lm75a_temp_ReadBytes((uint8_t *)board_lm75a_temp1,0,2,BOARD_LM75A92_ADDRESS)==1)
		{	
			float_temp = temp_calculate(board_lm75a_temp1); //采集转换成实际温度
			decimal_2_integer(float_temp,board_temp1); //整数化
			board_temp_valid[1] = 1;
			sprintf((char *)flash_buf,"Addr0x92:%d.%d C\r\n" ,board_temp1[0],board_temp1[1]);
		}
				else
		{
			board_temp_valid[1] = 0;
			sprintf((char *)flash_buf,"Addr0x92 TEMP read failed.\r\n");
		}
		strcat((char *)stPrintf_Buf.buf, flash_buf);
}

/*
*********************************************************************************************************
*	函 数 名: Board_ADDR94_temp
*	功能说明: 监测地址为0x94的温度芯片
*	形    参:  
*********************************************************************************************************
*/
void Board_ADDR94_temp(void)
{		
		float float_temp;
		char flash_buf[128] = {0};
		if(board_lm75a_temp_ReadBytes((uint8_t *)board_lm75a_temp2,0,2,BOARD_LM75A94_ADDRESS)==1)
		{	
			float_temp = temp_calculate(board_lm75a_temp2); //采集转换成实际温度
			decimal_2_integer(float_temp,board_temp2); //整数化
			board_temp_valid[2] = 1;
			sprintf((char *)flash_buf,"Addr0x94:%d.%d C\r\n" ,board_temp2[0],board_temp2[1]);
		}
				else
		{
			board_temp_valid[2] = 0;
			sprintf((char *)flash_buf,"Addr0x94 TEMP read failed.\r\n");
		}
		strcat((char *)stPrintf_Buf.buf, flash_buf);
}

/*
*********************************************************************************************************
*	函 数 名: board_lm75a_temp0_ReadBytes(模拟I2C读取数据)
*	功能说明: 从温度传感器的首地址读取两个字节的数据
*	形    参:  _usAddress : 起始地址
*			          _usSize : 数据长度,单位为字节
*			        _pReadBuf : 存放读到的数据的缓冲区指针
*	返 回 值: 0 表示失败,1表示成功
*********************************************************************************************************
*/
uint8_t board_lm75a_temp_ReadBytes(uint8_t *_pReadBuf, uint16_t _usAddress, uint16_t _usSize, uint8_t board_Lm75a_Address)
{
	uint16_t i;

	/* 第1步：发起I2C总线启动信号 */
	i2c_Start();

	/* 第2步：发起控制字节，高7bit是地址，bit0是读写控制位，0表示写，1表示读 */
	i2c_SendByte(board_Lm75a_Address | I2C_WR);	/* 此处是写指令 */

	/* 第3步：发送ACK */
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	
	}

	/* 第4步：发送字节地址，24C02只有256字节，因此1个字节就够了，如果是24C04以上，那么此处需要连发多个地址 */
		i2c_SendByte((uint8_t)_usAddress);
		if (i2c_WaitAck() != 0)
		{
			goto cmd_fail;	/* EEPROM器件无应答 */
		}
	/* 第6步：重新启动I2C总线。下面开始读取数据 */
	i2c_Start();

	/* 第7步：发起控制字节，高7bit是地址，bit0是读写控制位，0表示写，1表示读 */
	i2c_SendByte(board_Lm75a_Address | I2C_RD);	/* 此处是读指令 */

	/* 第8步：发送ACK */
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应答 */
	}

	/* 第9步：循环读取数据 */
	for (i = 0; i < _usSize; i++)
	{
		_pReadBuf[i] = i2c_ReadByte();	/* 读1个字节 */

		/* 每读完1个字节后，需要发送Ack， 最后一个字节不需要Ack，发Nack */
		if (i != _usSize - 1)
		{
			i2c_Ack();	/* 中间字节读完后，CPU产生ACK信号(驱动SDA = 0) */
		}
		else
		{
			i2c_NAck();	/* 最后1个字节读完后，CPU产生NACK信号(驱动SDA = 1) */
		}
	}
	/* 发送I2C总线停止信号 */
	i2c_Stop();
	return 1;	/* 执行成功 */

	cmd_fail: /* 命令执行失败后，切记发送停止信号，避免影响I2C总线上其他设备 */
	/* 发送I2C总线停止信号 */
	i2c_Stop();
	return 0;
}
/*
*********************************************************************************************************
*	函 数 名: decimal_2_integer
*	功能说明: 将ADC采集的电压信息整数化
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
int decimal_2_integer(float a,int b[2])
{
	float temp = 0.0;
	if(a<0){
		   a		= -a;
		   b[0]	= (int)(a);
		   temp	= a-b[0];
		   temp	*= 100;
		   b[1]	= (int)(temp);
		   b[0]	= -b[0];
		}
	else if(a>=0){
		   b[0]	= (int)(a);
		   temp	= a-b[0];
		   temp	*= 100;
		   b[1]	= (int)(temp);
	}
	return 0;
}
/*
*********************************************************************************************************
*	函 数 名: get_two_complement
*	功能说明: 求补码的源码(得到的值可能为负值)
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
uint16_t get_two_complement(uint16_t t_b)
{
	 int i = 0;
	 int tmp[11] = {0};
	 uint16_t r_bin = 0;
	 for(i=0;i<11;i++)
	 {
		   tmp[i] = (t_b>>i)%2;
		   if(tmp[i] == 0)
		       tmp[i] = 1;
		   else
		       tmp[i] = 0;
		  r_bin |= (tmp[i]<<i);
	 }
	 r_bin += 1;
	 return r_bin;
}
/*
*********************************************************************************************************
*	函 数 名: temp_calculate
*	功能说明: 将温度换算成实际数值
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
float temp_calculate(uint8_t buf[2])
{
	  uint16_t t_cpu		= 0;
	  float temp				= 0;
	
  	t_cpu	=((buf[0]<<8)|(buf[1]))>>5;//去除低五位
  	if(((t_cpu>>10)&1) == 0)//判断温度正负值
		         temp	= ((float)t_cpu)*0.125;
	  else if(((t_cpu>>10)&1) == 1)
			{
		      t_cpu	= get_two_complement(t_cpu);
		      temp	= -((float)t_cpu)*0.125;
	    }
	   return temp;
}
/*
*********************************************************************************************************
*	函 数 名: CalcChecksum
*	功能说明: 校验码验证
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
uint8_t CalcChecksum(uint8_t data[],int start,int end)
{
	uint8_t  Checksum = 0;
	int  i = 0;
	for(i =start; i < end; i++)
		Checksum = (Checksum + data[i]) % 256;
	return (-Checksum);
}
