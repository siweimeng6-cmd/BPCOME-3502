#include "bsp_init.h"
#include ".\lt8801\i2c_lt8801.h"
#include ".\lt8801\lt8801_drv.h"
xTaskHandle xExitLine0Semaphore    = NULL; 

TaskHandle_t HDMI0_SWICTH_Task_Handle = NULL;
TaskHandle_t USB_SWICTH_Task_Handle = NULL;
TaskHandle_t GPIO_Task_Handle = NULL;
TaskHandle_t CLOSE_Task_Handle = NULL;

uint8_t hdmi0_on_flag =1;		//HDMI0工作，初始态为HDMI0
uint8_t hdmi1_on_flag =0;		//HDMI1不工作

uint8_t usb_01_state=0;
uint8_t pre_usb0_state = 1;
uint8_t now_usb0_state = 1;
uint8_t pre_usb1_state = 1;
uint8_t now_usb1_state = 1;
uint8_t pre_3v3_state = 0;
uint8_t now_3v3_state = 0;
uint8_t p3v3_good_flag =0;
uint8_t p1v2_on = 0; 

uint16_t state_6710_cnt=0;
uint8_t close_cnt = 0;
uint8_t pre_touch_state,now_touch_state=0;
uint8_t touch_up_cnt=0;
uint8_t touch_state=0;
uint8_t close_en=0;
uint8_t open_en=0;
uint8_t key_down_en=1;

uint8_t hdmi_channel_cnt;
uint8_t pre_hdmi1_state,now_hdmi1_state = 1;
uint8_t hdmi1_switch_en = 1;
uint8_t hdmi1_switch_cnt = 0;
uint8_t hdmi_close_usben = 0;//HDMI切换时，需要短暂关闭USB34_EN使能3~4s，否则切换速度过慢
uint8_t hdmi_close_usben_cnt = 0;
uint16_t analog_en_cnt = 0;
uint8_t bl_power_off=0;
uint8_t bb_bl_power_off=0;
uint8_t pg_cnt=0;
uint8_t pre_PC6,now_PC6=1;
uint8_t pre_PC8,now_PC8=1;
uint8_t lock_PC6,lock_PC8=0;

/*																						
*********************************************************************************************************
*	函 数 名: hdmi_switch
*	功能说明: HDMI通道切换，为硬件选通信号切换
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
static void  hdmi_switch(void)
{
    if((hdmi0_on_flag==0)&&(hdmi1_on_flag==1))//HDMI1工作时
    {
                //点亮HDMI0 LED
                GPIO_SetBits(LED0_HDMI0_GPIO_PORT , LED0_HDMI0_GPIO_PIN);//点亮HDMI0指示灯
                GPIO_ResetBits(LED1_HDMI1_GPIO_PORT , LED1_HDMI1_GPIO_PIN);//熄灭HDMI1指示灯
                GPIO_ResetBits(MB_AB_NOE0_GPIO_PORT , MB_AB_NOE0_GPIO_PIN);
                GPIO_ResetBits(MB_AB_SEL0_GPIO_PORT , MB_AB_SEL0_GPIO_PIN);
                hdmi0_on_flag=1;//切换到HDMI0工作
                hdmi1_on_flag=0;//停止到HDMI1工作
                
                GPIO_ResetBits(USB_OUT0_GPIO_PORT, USB_OUT0_GPIO_PIN);
                GPIO_ResetBits(USB_OUT1_GPIO_PORT, USB_OUT1_GPIO_PIN);
                printf("切换HDMI0成功  \r\n");
        
     }						
     else if((hdmi0_on_flag==1)&&(hdmi1_on_flag==0))//HDMI0工作时
     {
                    //点亮HDMI1 LED
                    GPIO_SetBits(LED1_HDMI1_GPIO_PORT , LED1_HDMI1_GPIO_PIN);//点亮HDMI1指示灯
                    GPIO_ResetBits(LED0_HDMI0_GPIO_PORT , LED0_HDMI0_GPIO_PIN);//熄灭HDMI0指示灯
                    GPIO_ResetBits(MB_AB_NOE0_GPIO_PORT , MB_AB_NOE0_GPIO_PIN);
                    GPIO_SetBits(MB_AB_SEL0_GPIO_PORT , MB_AB_SEL0_GPIO_PIN);
                    hdmi0_on_flag=0;//停止到HDMI0工作
                    hdmi1_on_flag=1;//切换到HDMI1工作
                    
                    GPIO_SetBits(USB_OUT0_GPIO_PORT	, USB_OUT0_GPIO_PIN);
                    GPIO_ResetBits(USB_OUT1_GPIO_PORT	, USB_OUT1_GPIO_PIN);
                    printf("切换HDMI1成功  \r\n");
     }
     

}

/*																						
*********************************************************************************************************
*	函 数 名: 
*	功能说明: 
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
void HDMI0_SWICTH_Task(void* parameter)
{
while(1)
{
/**************************检测hdmi1************/
            if(GPIO_ReadInputDataBit(SWITCH_HDMI0_GPIO_PORT,SWITCH_HDMI0_GPIO_PIN)==0)	//检测低脉冲
            {
                    pre_hdmi1_state = now_hdmi1_state;
                    now_hdmi1_state =0;
                    hdmi1_switch_cnt = 0;
                    if((pre_hdmi1_state == 0)&&(now_hdmi1_state == 0)&&( hdmi1_switch_en ==1))
                    {
                        hdmi1_switch_en =0;
                        hdmi_switch();	
                        //HDMI切换后，短暂关闭USB34_EN,其为低有效
                        if((GPIO_ReadInputDataBit(USB34_EN_GPIO_PORT,USB34_EN_GPIO_PIN)==0)&&(hdmi_close_usben == 0))
                        {
                            GPIO_SetBits(USB34_EN_GPIO_PORT  , USB34_EN_GPIO_PIN);
                            hdmi_close_usben = 1;
                        }							
                    }
            }
            else if(GPIO_ReadInputDataBit(SWITCH_HDMI0_GPIO_PORT,SWITCH_HDMI0_GPIO_PIN)==1)//检测高电平2s，恢复按键使能
            {
                    pre_hdmi1_state = now_hdmi1_state;
                    now_hdmi1_state =1;
                    if((pre_hdmi1_state == 1)&&(now_hdmi1_state == 1))
                    {
                        hdmi1_switch_cnt++;
                        if(hdmi1_switch_cnt == 40)//置高2s
                        {
                            hdmi1_switch_cnt = 0;
                            hdmi1_switch_en =1;
                        }
                    }
            }
            //HDMI切换后，关闭USB34_EN后3~4s，再次开启USB34_EN
            if((GPIO_ReadInputDataBit(USB34_EN_GPIO_PORT,USB34_EN_GPIO_PIN)== 1)&&(hdmi_close_usben == 1))
            {
                hdmi_close_usben_cnt++;
                if(hdmi_close_usben_cnt == 120)//USB34_EN失能6s
                {
                    hdmi_close_usben_cnt = 0;
                    hdmi_close_usben = 0;
                    GPIO_ResetBits(USB34_EN_GPIO_PORT  , USB34_EN_GPIO_PIN);
                }
            }
            vTaskDelay(50);
}
}

/*																						
*********************************************************************************************************
*	函 数 名:  GPIO_Task
*	功能说明: 
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
void GPIO_Task(void* parameter)
{
stCHIP_STATUS_t *pstChip_Status;
while(1)
{
/*************查询3.3Vpower good的状态，来启动上电**********************/
        if(GPIO_ReadInputDataBit(P3V3_GD_GPIO_PORT,P3V3_GD_GPIO_PIN)==1)
        {
            pre_3v3_state = now_3v3_state;
            now_3v3_state =1;
            if(pg_cnt<10)
                pg_cnt ++;
            
            if((pre_3v3_state == 1)&& (now_3v3_state ==1)&&(p3v3_good_flag == 0))
            {
#if MCU_CONTROL_POWER		
                printf("P3V3 POWER GOOD\r\n");
                delay_1ms(10);
                GPIO_SetBits(P1V8_BB_DDR_EN_GPIO_PORT  , P1V8_BB_DDR_EN_GPIO_PIN);//PC2拉高
                delay_1ms(40);
                GPIO_SetBits(P1V_STB_EN_GPIO_PORT  , P1V_STB_EN_GPIO_PIN);//PC3拉高
                delay_1ms(50);
                GPIO_SetBits(P1V2_8801S_EN_GPIO_PORT  , P1V2_8801S_EN_GPIO_PIN);//PB13拉高
                printf("1V8上电！1V上电！1V2上电！\r\n");
                p1v2_on = 1;								
#endif
                
                delay_1ms(300);
                GPIO_SetBits  (EDP_POWER_1_GPIO_PORT,EDP_POWER_1_GPIO_PIN);
                GPIO_SetBits  (EDP_POWER_2_GPIO_PORT,EDP_POWER_2_GPIO_PIN);
                delay_1ms(1000);
                
                //上电开背光
                GPIO_SetBits  (BL_EN_GPIO_PORT	, BL_EN_GPIO_PIN);	//背光
                GPIO_SetBits  (BB_BL_EN_GPIO_PORT	, BB_BL_EN_GPIO_PIN);

                p3v3_good_flag =1;
            }
            
            if(GPIO_ReadInputDataBit(ANALOG_IN_GPIO_PORT,ANALOG_IN_GPIO_PIN)==1)
                GPIO_SetBits(ANALOG_EN_GPIO_PORT  , ANALOG_EN_GPIO_PIN);	
            else
                GPIO_ResetBits(ANALOG_EN_GPIO_PORT  , ANALOG_EN_GPIO_PIN);	
            
            //检测PD2 (MCU_GPIO0) 状态，控制PA0 (NS4251_SD_R)
            if(GPIO_ReadInputDataBit(MCU_GPIO0_GPIO_PORT,MCU_GPIO0_GPIO_PIN)==0)
            {
                //PD2为低时，PA0输出低
                GPIO_ResetBits(NS4251_SD_R_GPIO_PORT  , NS4251_SD_R_GPIO_PIN);
            }
            else
            {
                //PD2为高时，PA0输出高
                GPIO_SetBits(NS4251_SD_R_GPIO_PORT  , NS4251_SD_R_GPIO_PIN);
            }

        }
        else 
        {
            pre_3v3_state = now_3v3_state;
            now_3v3_state =0;
            if((pre_3v3_state == 0)&& (now_3v3_state ==0)&&(p3v3_good_flag == 1))
            {
                GPIO_SetBits(P1V8_BB_DDR_EN_GPIO_PORT  , P1V8_BB_DDR_EN_GPIO_PIN);//PC2拉高
                GPIO_SetBits(P1V_STB_EN_GPIO_PORT  , P1V_STB_EN_GPIO_PIN);//PC3拉高
                GPIO_SetBits(P1V2_8801S_EN_GPIO_PORT  , P1V2_8801S_EN_GPIO_PIN);//PB13拉高
                printf("1V8下电！1V下电！1V2下电！\r\n");
                p1v2_on = 0;
            }
        }

        
#if 1		
/*************每30s查询一次6710的工作状态**********************/
        state_6710_cnt ++;
        if(state_6710_cnt == 300)	
        {
                state_6710_cnt = 0;
                if(GPIO_ReadInputDataBit(LED_GREEN_GPIO_PORT,LED_GREEN_GPIO_PIN)==1)
                    printf("PB3_6710_RUN\r\n");
                else
                    printf("PB3_6710_NO_RUN\r\n");
                
                if(GPIO_ReadInputDataBit(LED_GREEN_6710B_GPIO_PORT,LED_GREEN_6710B_GPIO_PIN)==1)
                    printf("PA1_6710_RUN\r\n");
                else
                    printf("PA1_6710_NO_RUN\r\n");
        }
#endif	
        if(GPIO_ReadInputDataBit(CLOSE_GPIO_PORT,CLOSE_GPIO_PIN)==1)//开盖状态
        {
                //PC6,BB BL
                if(GPIO_ReadInputDataBit(BB_BL_PWM_GPIO_PORT,BB_BL_PWM_GPIO_PIN)==0)	//PC6
                {
                        pre_PC6 = now_PC6;
                        now_PC6 = 0;
                        if((pre_PC6 == 0)&&(now_PC6 == 0)&&(lock_PC6 == 0))
                        {
                                lock_PC6=1;
                                if(bb_bl_power_off==0)//开背光
                                {
                                    GPIO_ResetBits( BB_BL_EN_GPIO_PORT ,  BB_BL_EN_GPIO_PIN);					//PC7
                                    bb_bl_power_off=1;
                                    printf("BB_BL power off\r\n");
                                }
                                else								//关背光
                                {
                                    GPIO_SetBits( BB_BL_EN_GPIO_PORT ,  BB_BL_EN_GPIO_PIN);
                                    bb_bl_power_off=0;
                                    printf("BB_BL power on\r\n");
                                }
                        }
                }
                else
                {
                        pre_PC6 = now_PC6;
                        now_PC6 = 1;
                        lock_PC6=0;
                }
                
                //PC8,BL
                if(GPIO_ReadInputDataBit(BL_PWM_GPIO_PORT,BL_PWM_GPIO_PIN)==0)				//PC8
                {
                        pre_PC8 = now_PC8;
                        now_PC8 = 0;
                        if((pre_PC8 == 0)&&(now_PC8 == 0)&&(lock_PC8 == 0))
                        {
                                lock_PC8=1;
                                if(bl_power_off==0)//开背光
                                {
                                    GPIO_ResetBits( BL_EN_GPIO_PORT ,  BL_EN_GPIO_PIN);								//PC9
                                    bl_power_off=1;
                                    printf("BL power off\r\n");
                                }
                                else								//关背光
                                {
                                    GPIO_SetBits( BL_EN_GPIO_PORT ,  BL_EN_GPIO_PIN);								//PC9
                                    bl_power_off=0;
                                    printf("BL power on\r\n");
                                }
                        }
                }
                else
                {
                        pre_PC8 = now_PC8;
                        now_PC8 = 1;
                        lock_PC8=0;
            }	
        }			
        
        
        vTaskDelay(100);
}
}

/*																						
*********************************************************************************************************
*	函 数 名:  GPIO_Task
*	功能说明: USB34_EN为低有效
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
void CLOSE_Task(void* parameter)
{
while(1)
{
/*************检测盒盖输入信号，高于5s则失能触摸**********************/
//开盖状态，若touch switch有低脉冲则拉高,再次低脉冲则拉低
//合盖状态，超过5s则拉低
        if(GPIO_ReadInputDataBit(CLOSE_GPIO_PORT,CLOSE_GPIO_PIN)==1)//开盖状态
        {
                close_cnt = 0;
                if(bl_power_off == 0)	//电源按键未关背光
                    GPIO_SetBits  (BL_EN_GPIO_PORT	, BL_EN_GPIO_PIN);	//背光
                if(bb_bl_power_off == 0)	//电源按键未关背光
                    GPIO_SetBits  (BB_BL_EN_GPIO_PORT	, BB_BL_EN_GPIO_PIN);
            
                //状态1：由合盖转为开盖，则打开USB使能
                if(open_en == 0)	
                {
                        touch_state =0;
                        GPIO_ResetBits(USB34_EN_GPIO_PORT  , USB34_EN_GPIO_PIN);
                        GPIO_ResetBits(TOUCH_LED_GPIO_PORT  , TOUCH_LED_GPIO_PIN);	//低电平点灯
                        printf("USB34拉低\r\n");
                        open_en =1;
                }		
                //状态2：开盖状态下，touch switch低脉冲
                if((GPIO_ReadInputDataBit(TOUCH_SW_GPIO_PORT,TOUCH_SW_GPIO_PIN)==0))//touch swtich低脉冲,则拉高PB1
                {
                    pre_touch_state = now_touch_state;
                    now_touch_state = 0;
                    touch_up_cnt=0;
                    if((pre_touch_state == 0)&&(now_touch_state == 0)&&(close_en == 1))//50ms判断到两次低电平，则表明有脉冲，拉高
                    {
                            close_en = 0;
                            if(touch_state == 0)
                            {
                                touch_state =1;
                                GPIO_SetBits(USB34_EN_GPIO_PORT  , USB34_EN_GPIO_PIN);
                                GPIO_SetBits(TOUCH_LED_GPIO_PORT  , TOUCH_LED_GPIO_PIN);
                                printf("USB34拉高\r\n");
                            }
                            else
                            {
                                touch_state =0;
                                GPIO_ResetBits(USB34_EN_GPIO_PORT  , USB34_EN_GPIO_PIN);
                                GPIO_ResetBits(TOUCH_LED_GPIO_PORT  , TOUCH_LED_GPIO_PIN);	//低电平点灯
                                printf("USB34拉低\r\n");
                            }
                    }
                }
                else if(GPIO_ReadInputDataBit(TOUCH_SW_GPIO_PORT,TOUCH_SW_GPIO_PIN)==1)		////touch swtich为高
                {
                    touch_up_cnt++;
                    if((touch_up_cnt==10)&&(close_en == 0))
                    {
                        close_en =1;
                        printf("en open\r\n");
                        touch_up_cnt=0;
                    }
                }
            
        }
        else																												//合盖状态
        {
            close_cnt++;
            if(close_cnt == 100)
            {
                GPIO_SetBits(USB34_EN_GPIO_PORT  , USB34_EN_GPIO_PIN);					//盒盖超过5s，则拉高PB1
                GPIO_SetBits(TOUCH_LED_GPIO_PORT  , TOUCH_LED_GPIO_PIN);
                open_en=0;
                GPIO_ResetBits  (BL_EN_GPIO_PORT	, BL_EN_GPIO_PIN);	//背光
                GPIO_ResetBits  (BB_BL_EN_GPIO_PORT	, BB_BL_EN_GPIO_PIN);
            }
        }
        vTaskDelay(50);
}
}

