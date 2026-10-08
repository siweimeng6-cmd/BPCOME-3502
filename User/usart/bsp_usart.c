#include "bsp_usart.h"
#include "gpio/bsp_gpio.h"
#include "bsp_mo_i2c.h"
#include <string.h>

#define RESET_CMD   "Reset"
#define CLEAR_RUNTIME_CMD "ClearRuntime"

/* 两路接收各自组行；ISR仅排队。重复请求在结果发回前合并，避免重复擦写。 */
typedef struct {
    char line[16];
    uint8_t used;
    uint8_t discard;
    uint8_t clear_blocked; /* 本行开始时已有清零请求，不能跨回执边界成为新请求。 */
} SerialCommandLine;

#define COMMAND_IDLE  0
#define COMMAND_WAIT  1
#define COMMAND_OK    2
#define COMMAND_ERROR 3
static SerialCommandLine s_command_lines[2];
static volatile uint8_t s_command_result[2];
static volatile uint8_t s_clear_request_mask;

static void Serial_CommandByte(uint8_t port, uint8_t ch, BaseType_t *woken)
{
    SerialCommandLine *line = &s_command_lines[port];
    if (ch == '\r' || ch == '\n')
    {
        line->line[line->used] = '\0';
        if (!line->discard && line->used)
        {
            if (strcmp(line->line, CLEAR_RUNTIME_CMD) == 0)
            {
                if (!line->clear_blocked && s_command_result[port] == COMMAND_IDLE)
                {
                    s_command_result[port] = COMMAND_WAIT;
                    s_clear_request_mask |= (uint8_t)(1U << port);
                }
            }
            else if (port == SERIAL_COMMAND_DEBUG && strcmp(line->line, RESET_CMD) == 0 &&
                     xSelfResetSemaphore != NULL)
                xSemaphoreGiveFromISR(xSelfResetSemaphore, woken);
        }
        line->used = 0;
        line->discard = 0;
        line->clear_blocked = 0;
    }
    else if (!line->discard)
    {
        if (line->used == 0)
            line->clear_blocked = (s_command_result[port] != COMMAND_IDLE);
        if (ch == 0 || line->used >= sizeof(line->line) - 1)
            line->discard = 1; /* 超长/含NUL的整行丢弃，不能将尾部误当新指令。 */
        else
            line->line[line->used++] = (char)ch;
    }
}

static void Serial_CommandIRQ(USART_TypeDef *uart, uint8_t port)
{
    BaseType_t woken = pdFALSE;
    uint16_t status = (uint16_t)uart->SR;
    if (status & (USART_FLAG_RXNE | USART_FLAG_ORE | USART_FLAG_NE | USART_FLAG_FE | USART_FLAG_PE))
    {
        uint8_t ch = (uint8_t)USART_ReceiveData(uart); /* 先读SR再读DR，同时清除接收错误 */
        if (status & (USART_FLAG_ORE | USART_FLAG_NE | USART_FLAG_FE | USART_FLAG_PE))
            s_command_lines[port].discard = 1;
        Serial_CommandByte(port, ch, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

/* Sensor_Task独占模拟I2C。CPU请求也由此执行，UART5_Task只负责发送结果。 */
void Serial_ProcessRuntimeCommands(void)
{
    uint8_t requested, result, port;
    taskENTER_CRITICAL();
    requested = s_clear_request_mask;
    taskEXIT_CRITICAL();
    if (!requested)
        return;

    result = Runtime_ClearAll() ? COMMAND_OK : COMMAND_ERROR;
    taskENTER_CRITICAL();
    /* 擦除期间另一串口发来的同一指令并入本次执行。 */
    for (port = 0; port < 2; ++port)
        if (s_clear_request_mask & (1U << port))
            s_command_result[port] = result;
    s_clear_request_mask = 0;
    taskEXIT_CRITICAL();
}

/* 每个串口由其原有发送任务调用，CPU回执不会插入健康报文中间。 */
void Serial_SendCommandReply(uint8_t port)
{
    uint8_t result, sent;
    char *reply;
    if (port > SERIAL_COMMAND_CPU)
        return;
    taskENTER_CRITICAL();
    result = s_command_result[port];
    taskEXIT_CRITICAL();
    if (result == COMMAND_OK)
        reply = "\r\nClearRuntime:OK\r\n";
    else if (result == COMMAND_ERROR)
        reply = "\r\nClearRuntime:ERROR EEPROM\r\n";
    else
        return;

    /* 保持忙状态到最后一个停止位发完；失败仅重发回执，不重复擦除。 */
    sent = Usart_SendString(port == SERIAL_COMMAND_DEBUG ? DEBUG_USARTx : HEALTH_USARTx, reply);
    if (sent)
    {
        taskENTER_CRITICAL();
        s_command_result[port] = COMMAND_IDLE;
        taskEXIT_CRITICAL();
    }
}

/* DEBUG TX serialization: all task senders use the same priority-inheriting mutex. */
#define USART_TX_WAIT_LOOPS 100000UL
static SemaphoreHandle_t s_debug_tx_mutex;

/* 0: cannot send, 1: owns DEBUG mutex, 2: single-writer/no scheduler yet. */
static uint8_t Usart_TxAcquire(USART_TypeDef *uart)
{
    BaseType_t scheduler;
    TickType_t wait_ticks;
    /* ISR/异常内不打印、不操作互斥量，避免高优先级中断阻塞tick。 */
    if (xPortIsInsideInterrupt() != pdFALSE)
        return 0;
    if (uart != DEBUG_USARTx)
        return 2;
    scheduler = xTaskGetSchedulerState();
    if (scheduler == taskSCHEDULER_NOT_STARTED)
        return 2;
    if (s_debug_tx_mutex == NULL)
        return 0;
    /* 启动任务的临界区、暂停调度和断言路径只能立即尝试，不能阻塞。 */
    wait_ticks = (scheduler == taskSCHEDULER_RUNNING &&
                  __get_PRIMASK() == 0 && __get_BASEPRI() == 0) ? portMAX_DELAY : 0;
    return xSemaphoreTake(s_debug_tx_mutex, wait_ticks) == pdTRUE ? 1 : 0;
}

static void Usart_TxRelease(uint8_t lock)
{
    if (lock == 1)
        xSemaphoreGive(s_debug_tx_mutex);
}

static uint8_t Usart_WaitTxFlag(USART_TypeDef *uart, uint16_t flag)
{
    uint32_t remaining = USART_TX_WAIT_LOOPS;
    while (USART_GetFlagStatus(uart, flag) == RESET)
    {
        if (--remaining == 0)
            return 0;
    }
    return 1;
}

static uint8_t Usart_SendByteUnlocked(USART_TypeDef *uart, uint8_t ch)
{
    /* 先等TXE再写DR，防止接续另一个发送者时覆盖尚未移出的字节。 */
    if (!Usart_WaitTxFlag(uart, USART_FLAG_TXE))
        return 0;
    USART_SendData(uart, ch);
    return 1;
}

void USART_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    if (s_debug_tx_mutex == NULL)
        s_debug_tx_mutex = xSemaphoreCreateMutex();

    // 打开串口GPIO的时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    // 打开串口外设的时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);

    // 将USART Tx的GPIO配置为推挽复用模式
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // 将USART Rx的GPIO配置为浮空输入模式
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // 配置串口的工作参数
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(UART4, &USART_InitStructure);

    // 使能串口
    USART_Cmd(UART4, ENABLE);

    // 使能接收中断，用于监听Reset/ClearRuntime命令。
    USART_ITConfig(UART4, USART_IT_RXNE, ENABLE);
}

/***********************************************************************
* @ 函数名  DEBUG_USART_IRQHandler (即 UART4_IRQHandler)
* @ 功能说明  按行接收Reset/ClearRuntime；中断中不打印、不访问EEPROM。
*********************************************************************/
void DEBUG_USART_IRQHandler(void)
{
    Serial_CommandIRQ(DEBUG_USARTx, SERIAL_COMMAND_DEBUG);
}

void UART5_IRQHandler(void)
{
    Serial_CommandIRQ(HEALTH_USARTx, SERIAL_COMMAND_CPU);
}

/***********************************************************************
* @ 函数名  HEALTH_USART_Config
* @ 功能说明  PC12(TX)/PD2(RX) UART5 初始化：与核心卡串口0互连，健康上报口
*             （对照Sheet3「定义」SER_RX0/SER_TX0，Sheet1标注为UART5）。
*             具体上报的数据内容/协议Sheet3未给出，这里只把外设跑起来，
*             发送内容由 UART5_SendHealthReport() 的调用方决定。
*********************************************************************/
void HEALTH_USART_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // 打开串口GPIO的时钟
    RCC_APB2PeriphClockCmd(HEALTH_USART_TX_GPIO_CLK | HEALTH_USART_RX_GPIO_CLK, ENABLE);
    // 打开串口外设的时钟
    RCC_APB1PeriphClockCmd(HEALTH_USART_CLK, ENABLE);

    // PC12 - TX 推挽复用
    GPIO_InitStructure.GPIO_Pin = HEALTH_USART_TX_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(HEALTH_USART_TX_GPIO_PORT, &GPIO_InitStructure);

    // PD2 - RX 浮空输入
    GPIO_InitStructure.GPIO_Pin = HEALTH_USART_RX_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(HEALTH_USART_RX_GPIO_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = HEALTH_USART_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(HEALTH_USARTx, &USART_InitStructure);

    USART_Cmd(HEALTH_USARTx, ENABLE);
    USART_ITConfig(HEALTH_USARTx, USART_IT_RXNE, ENABLE);
}

/***********************************************************************
* @ 函数名  UART5_SendHealthReport
* @ 功能说明  向核心卡发送健康上报内容（占位：具体协议/内容待定，先按字符串发送）
*********************************************************************/
void UART5_SendHealthReport(char *str)
{
    Usart_SendString(HEALTH_USARTx, str);
}

// UART TX entry points: keep complete strings/arrays inside one lock.
int fputc(int ch, FILE *f)
{
    uint8_t lock = Usart_TxAcquire(DEBUG_USARTx);
    uint8_t sent;
    (void)f;
    if (!lock)
        return EOF;
    sent = Usart_SendByteUnlocked(DEBUG_USARTx, (uint8_t)ch);
    Usart_TxRelease(lock);
    return sent ? ch : EOF;
}

void Usart_SendByte(USART_TypeDef *pUSARTx, uint8_t ch)
{
    uint8_t lock = Usart_TxAcquire(pUSARTx);
    if (!lock)
        return;
    (void)Usart_SendByteUnlocked(pUSARTx, ch);
    Usart_TxRelease(lock);
}

void Usart_SendArray(USART_TypeDef *pUSARTx, uint8_t *array, uint16_t num)
{
    uint16_t i;
    uint8_t lock = Usart_TxAcquire(pUSARTx);
    if (!lock)
        return;
    for (i = 0; i < num; ++i)
    {
        if (!Usart_SendByteUnlocked(pUSARTx, array[i]))
            break;
    }
    (void)Usart_WaitTxFlag(pUSARTx, USART_FLAG_TC);
    Usart_TxRelease(lock);
}

/* 返回1表示TC确认整串已发完；失败仍释放互斥量，由命令层保留待发结果。 */
uint8_t Usart_SendString(USART_TypeDef *pUSARTx, char *str)
{
    uint8_t lock = Usart_TxAcquire(pUSARTx);
    uint8_t sent = 1;
    if (!lock)
        return 0;
    while (*str)
    {
        if (!Usart_SendByteUnlocked(pUSARTx, (uint8_t)*str++))
        {
            sent = 0;
            break;
        }
    }
    if (sent)
        sent = Usart_WaitTxFlag(pUSARTx, USART_FLAG_TC);
    Usart_TxRelease(lock);
    return sent;
}

void Usart_SendHalfWord(USART_TypeDef *pUSARTx, uint16_t ch)
{
    uint8_t bytes[2];
    bytes[0] = (uint8_t)(ch >> 8);
    bytes[1] = (uint8_t)ch;
    Usart_SendArray(pUSARTx, bytes, 2);
}

///重定向c库函数scanf到串口，重写向后可使用scanf、getchar等函数
int fgetc(FILE *f)
{
    /* 等待串口输入数据 */
    while (USART_GetFlagStatus(DEBUG_USARTx, USART_FLAG_RXNE) == RESET);

    return (int)USART_ReceiveData(DEBUG_USARTx);
}

