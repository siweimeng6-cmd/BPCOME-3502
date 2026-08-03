#include "bsp_adc.h"
#include "bsp_init.h"

__IO uint16_t ADC_ConvertedValue[ADC_CONVERT_CHANNEL*MAX_ADC_COLL_CNTS]={0};
stADC_DATA_t  stADC_Data;  


static void ADCx_GPIO_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	//  ADC IO
	ADC_GPIO_APBxClock_FUN ( RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE );
	
	//  PA7 - ADC1 P12V
	GPIO_InitStructure.GPIO_Pin = ADC_CNT1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(ADC_CNT1_PORT, &GPIO_InitStructure);
	
	//  PB0 - ADC2 P5V
	GPIO_InitStructure.GPIO_Pin = ADC_CNT2;
	GPIO_Init(ADC_CNT2_PORT, &GPIO_InitStructure);
	
	//  PB1 - ADC3 P3V3
	GPIO_InitStructure.GPIO_Pin = ADC_CNT3;
	GPIO_Init(ADC_CNT3_PORT, &GPIO_InitStructure);

	//  PC4 - ADC4 MT9221CT_OUT 电流监测
	GPIO_InitStructure.GPIO_Pin = ADC_CNT4;
	GPIO_Init(ADC_CNT4_PORT, &GPIO_InitStructure);
}


static void ADCx_Mode_Config(void)
{
	DMA_InitTypeDef DMA_InitStructure;
	ADC_InitTypeDef ADC_InitStructure;
	
	RCC_AHBPeriphClockCmd(ADC_DMA_CLK, ENABLE); 
	ADC_APBxClock_FUN ( ADC_CLK, ENABLE );	
	
	DMA_DeInit(ADC_DMA_CHANNEL);
	
	DMA_InitStructure.DMA_PeripheralBaseAddr = ( u32 ) ( & ( ADC_x->DR ) );	
	DMA_InitStructure.DMA_MemoryBaseAddr = (u32)ADC_ConvertedValue;	
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize = ADC_CONVERT_CHANNEL*MAX_ADC_COLL_CNTS;	
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;	
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable; 	
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;	
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;	
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;		
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;	
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;	
	DMA_Init(ADC_DMA_CHANNEL, &DMA_InitStructure);	
	
	//  DMA 
	DMA_Cmd(ADC_DMA_CHANNEL , ENABLE);
	
	// ADC 
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;	
	ADC_InitStructure.ADC_ScanConvMode = ENABLE ; 
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;	
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;	
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;	
	ADC_InitStructure.ADC_NbrOfChannel = ADC_CONVERT_CHANNEL;		// 
	ADC_Init(ADC_x, &ADC_InitStructure);	//
	
	RCC_ADCCLKConfig(RCC_PCLK2_Div8); 	// 
	
	ADC_RegularChannelConfig(ADC_x, ADC_CNT1_CHANNEL		, 1, ADC_SampleTime_55Cycles5);
	ADC_RegularChannelConfig(ADC_x, ADC_CNT2_CHANNEL		, 2, ADC_SampleTime_55Cycles5);
	ADC_RegularChannelConfig(ADC_x, ADC_CNT3_CHANNEL		, 3, ADC_SampleTime_55Cycles5);
	ADC_RegularChannelConfig(ADC_x, ADC_CNT4_CHANNEL		, 4, ADC_SampleTime_55Cycles5);
	
	ADC_DMACmd(ADC_x, ENABLE);									
	
	ADC_Cmd(ADC_x, ENABLE);											
	
	ADC_ResetCalibration(ADC_x);									  
	while(ADC_GetResetCalibrationStatus(ADC_x));	
	
	ADC_StartCalibration(ADC_x);									
	while(ADC_GetCalibrationStatus(ADC_x));				
	
	ADC_SoftwareStartConvCmd(ADC_x, ENABLE);			
}


void ADC_Filter( void )
{
	uint32_t adc_aver_sum[ADC_CONVERT_CHANNEL]={0};			
	uint16_t i = 0, j = 0;
	
	for(i = 0; i < (MAX_ADC_COLL_CNTS * ADC_CONVERT_CHANNEL); i += ADC_CONVERT_CHANNEL){
		for(j = 0; j < ADC_CONVERT_CHANNEL; j++)
		  adc_aver_sum[j] += ADC_ConvertedValue[i+j];
	}
	
	for(i = 0; i < ADC_CONVERT_CHANNEL; i++){
		stADC_Data.adc_filter_val[i] = adc_aver_sum[i] / MAX_ADC_COLL_CNTS;
	}
}


void ADCx_Init(void)
{
	ADCx_GPIO_Config();
	ADCx_Mode_Config();
}


void ADC_Calculation(void)
{
    uint8_t i = 0;
    char flash_buf[128] = {0};
    
    ADC_Filter();
    for( i = 0; i < ADC_CONVERT_CHANNEL; i++ )
    {
            if(i==0){		//ADC1 P12V
                // 1K/(11K+1K) = 1/12
                stADC_Data.adc_voltage_val[i]=stADC_Data.adc_filter_val[i]/ADC_PRE_12BIT*ADC_VREF*12;
                sprintf((char *)flash_buf,"V_12V:%.2fV\r\n",stADC_Data.adc_voltage_val[i]);
            }
            else if(i==1){		//ADC2 P5V    4.8
                // 1.24K/(4.7K+1.24K) = 1.24/5.94
                stADC_Data.adc_voltage_val[i]=stADC_Data.adc_filter_val[i]/ADC_PRE_12BIT*ADC_VREF/1.24*(1.24+4.7);
                sprintf((char *)flash_buf,"V_5V:%.2fV\r\n",stADC_Data.adc_voltage_val[i]);
            }
            else if(i==2){		//ADC3 P3V3    3.2
                // 1K/(2.2K+1K) = 1/3.2
                stADC_Data.adc_voltage_val[i]=stADC_Data.adc_filter_val[i]/ADC_PRE_12BIT*ADC_VREF*3.2;
                sprintf((char *)flash_buf,"V_3.3V:%.2fV\r\n",stADC_Data.adc_voltage_val[i]);
            }
            else if(i==3){		//ADC4 MT9221CT_OUT 电流监测：VCC=5V，DC模式，敏感度0.2V/A，效率90%
                float mt9221_vol = stADC_Data.adc_filter_val[i]/ADC_PRE_12BIT*ADC_VREF;
                stADC_Data.adc_voltage_val[i] = (mt9221_vol - MT9221_VOL_BASE) / MT9221_SENSITIVITY * MT9221_EFFICIENCY;
                if(stADC_Data.adc_voltage_val[i] < 0)
                    stADC_Data.adc_voltage_val[i] = 0;
                sprintf((char *)flash_buf,"I_CURRENT:%.2fA\r\n",stADC_Data.adc_voltage_val[i]);
            }
            strcat((char *)stPrintf_Buf.buf, flash_buf);
            memset(flash_buf, 0, 128);

    }
}
