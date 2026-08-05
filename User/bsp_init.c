#include "bsp_init.h"
#include "task_usart.h"
#include "bsp_adc.h"
#include "bsp_mo_i2c.h"
#include "usart/bsp_usart.h"

// 任务句柄
extern TaskHandle_t GPIO_Task_Handle;

// 栈溢出处理函数
void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName)
{
    // 打印栈溢出的任务名称
    printf("栈溢出！任务: %s\r\n", pcTaskName);
    
    // 可以添加其他处理代码，如复位系统等
    // NVIC_SystemReset();
}




extern void vPortSetupTimerInterrupt( void );


static void NVIC_Configuration(void)
{
  NVIC_InitTypeDef NVIC_InitStructure;
  
  NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 );
  
    // 配置UART4中断优先级
    NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
}
/***********************************************************************
  * @ 函数名  ： BSP_Init
  * @ 功能说明： 板级外设初始化，所有板子上的初始化均可放在这个函数里面
  * @ 参数    ：   
  * @ 返回值  ： 无
  *********************************************************************/
void BSP_Init(void)
{
    NVIC_Configuration();
    
    USART_Config();

    HEALTH_USART_Config();

    bsp_gpio_init();
    
    BASIC_TIM_Mode_Config();
    
    ADCx_Init();
    
    pwm_init();
    tach_init();
    
    Init_MO_I2C();
    printf("\r\n");
    printf("初始化完成\r\n");
    printf("\r\n");
    eeprom_test();

    Runtime_Init();

    delay_1ms(100);
}

/***********************************************************************
  * @ 函数名  ： Delay_ms
  * @ 功能说明： 操作系统延时delay
  * @ 参数    ： 无  
  * @ 返回值  ： 无
  **********************************************************************/
void delay_1ms(uint32_t count)
{
       u32 ticks;
       u32 told,tnow,reload,tcnt=0;
       if((0x0001&(SysTick->CTRL)) ==0)    //定时器未工作
              vPortSetupTimerInterrupt();  //初始化定时器
 
       reload = SysTick->LOAD;                     //获取重装载寄存器值
       ticks = count * (SystemCoreClock / 1000);  //计数时间值
       
       vTaskSuspendAll();//阻止OS调度，防止打断us延时
       told=SysTick->VAL;  //获取当前数值寄存器值（开始时数值）
       while(1)
       {
              tnow=SysTick->VAL; //获取当前数值寄存器值
              if(tnow!=told)  //当前值不等于开始值说明已在计数
              {         
                     if(tnow<told)  //当前值小于开始数值，说明未计到0
                          tcnt+=told-tnow; //计数值=开始值-当前值
 
                     else     //当前值大于开始数值，说明已计到0并重新计数
                            tcnt+=reload-tnow+told;   //计数值=重装载值-当前值+开始值  （
                                                      //已从开始值计到0） 
 
                     told=tnow;   //更新开始值
                     if(tcnt>=ticks)break;  //时间超过/等于要延迟的时间,则退出.
              } 
       }  
       xTaskResumeAll();	//恢复OS调度

}

/***********************************************************************
  * @ 函数名  ： AppTaskCreate
  * @ 功能说明： 为了方便管理，所有的任务创建函数都放在这个函数里面
  * @ 参数    ： 无  
  * @ 返回值  ： 无
  **********************************************************************/
void AppTaskCreate(void)
{
  BaseType_t xReturn = pdPASS;/* 定义一个创建信息返回值，默认为pdPASS */
  
  taskENTER_CRITICAL();           //进入临界区   
  
/**************************************************************************************************/  
	xReturn = xTaskCreate((TaskFunction_t )Sensor_Task, /* 任务入口函数 */
                        (const char*    )"Sensor_Task",/* 任务名字 */
                        (uint16_t       )512,   /* 任务栈大小 */
                        (void*          )NULL,	/* 任务入口函数参数 */
                        (UBaseType_t    )2,	/* 任务的优先级 */
                        (TaskHandle_t*  )&Sensor_Task_Handle);/* 任务控制块指针 */
  if(pdPASS == xReturn)
    printf("创建Sensor_Task任务成功!\r\n"); 
  else
    printf("创建Sensor_Task任务失败!\r\n");   
/**************************************************************************************************/  
  // 创建GPIO_Task任务
  xReturn = xTaskCreate((TaskFunction_t )GPIO_Task, /* 任务入口函数 */
                        (const char*    )"GPIO_Task",/* 任务名字 */
                        (uint16_t       )512,   /* 任务栈大小 */
                        (void*          )NULL,	/* 任务入口函数参数 */
                        (UBaseType_t    )3,	/* 任务的优先级 */
                        (TaskHandle_t*  )&GPIO_Task_Handle);/* 任务控制块指针 */
  if(pdPASS == xReturn)
    printf("创建GPIO_Task任务成功!\r\n");
  else
    printf("创建GPIO_Task任务失败!\r\n");
/**************************************************************************************************/
  // 创建UART5_Task任务，通过UART5(PC12/PD2)向核心卡做健康上报
  // 优先级设为2，不能高于GPIO_Task(3)，避免挤占PWROK电源时序和自复位脉冲的响应
  xReturn = xTaskCreate((TaskFunction_t )UART5_Task, /* 任务入口函数 */
                        (const char*    )"UART5_Task",/* 任务名字 */
                        (uint16_t       )512,   /* 任务栈大小 */
                        (void*          )NULL,	/* 任务入口函数参数 */
                        (UBaseType_t    )2,	/* 任务的优先级 */
                        (TaskHandle_t*  )&UART5_Task_Handle);/* 任务控制块指针 */
  if(pdPASS == xReturn)
    printf("创建UART5_Task任务成功!\r\n");
  else
    printf("创建UART5_Task任务失败!\r\n");

  taskEXIT_CRITICAL();//退出临界区
  vTaskDelete(NULL); //删除当前任务
}

