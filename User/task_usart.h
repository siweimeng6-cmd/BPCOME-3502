#ifndef  __TASK_H
#define	 __TASK_H
#include "bsp_init.h"

extern TaskHandle_t Sensor_Task_Handle;
extern TaskHandle_t UART5_Task_Handle;


void Sensor_Task(void* parameter);


#endif 