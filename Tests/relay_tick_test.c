#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef uint32_t TickType_t;
typedef struct { unsigned level; } GPIO_TypeDef;
#define RELAY_TIMEBASE_TIM 1
#define RELAY_SIGNAL_TIMEOUT_MS 1000
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define Bit_SET 1
#define INCLUDE_xTaskGetSchedulerState 1
#define taskSCHEDULER_NOT_STARTED 0
static int scheduler_started, in_high_priority_irq;
static unsigned from_isr_calls;
static TickType_t kernel_tick;
static uint16_t timer_counter;
static int xTaskGetSchedulerState(void) { return scheduler_started; }
static void xPortSysTickHandler(void) { ++kernel_tick; }
static TickType_t xTaskGetTickCount(void) { return kernel_tick; }
static TickType_t xTaskGetTickCountFromISR(void)
{
    assert(!in_high_priority_irq); /* Reproduces the actual FreeRTOS priority rule. */
    ++from_isr_calls;
    return kernel_tick;
}
static uint16_t TIM_GetCounter(int timer) { (void)timer; return timer_counter; }
static unsigned GPIO_ReadInputDataBit(GPIO_TypeDef *port, uint16_t pin)
{ (void)pin; return port->level; }
static void GPIO_SetBits(GPIO_TypeDef *port, uint16_t pin)
{ (void)pin; port->level = 1; }
static void GPIO_ResetBits(GPIO_TypeDef *port, uint16_t pin)
{ (void)pin; port->level = 0; }

#include "relay_impl.inc"
#include "systick_impl.inc"

static void edge(GPIO_TypeDef *input, GPIO_TypeDef *output, unsigned level, uint16_t counter)
{
    input->level = level;
    timer_counter = counter;
    in_high_priority_irq = 1;
    Relay_ProcessEdge(&g_fan_tach_relay, input, 1, output, 1);
    in_high_priority_irq = 0;
    assert(output->level == level);
}
int main(void)
{
    GPIO_TypeDef input = {0}, output = {0};
    unsigned i, calls_before;
    SysTick_Handler();
    assert(g_relay_tick_snapshot == 0 && from_isr_calls == 0);
    scheduler_started = 1;
    kernel_tick = UINT32_MAX;
    SysTick_Handler(); /* Mirror must match the tick after wrap. */
    assert(g_relay_tick_snapshot == 0);
    SysTick_Handler();
    assert(g_relay_tick_snapshot == 1);
    edge(&input, &output, 1, 65000);
    edge(&input, &output, 0, 65500);
    edge(&input, &output, 1, 464);
    assert(g_fan_tach_relay.period_ticks == 1000);
    assert(g_fan_tach_relay.high_ticks == 500 && fan_tach_signal_is_active());
    calls_before = from_isr_calls;
    for (i = 0; i < 100000; ++i)
        edge(&input, &output, i & 1, (uint16_t)i);
    assert(from_isr_calls == calls_before); /* No RTOS call on any relay edge. */
    for (i = 0; i < 1001; ++i) SysTick_Handler();
    assert(!fan_tach_signal_is_active());
    edge(&input, &output, 1, 1000);
    assert(!g_fan_tach_relay.have_period);
    edge(&input, &output, 0, 1500);
    edge(&input, &output, 1, 2000);
    assert(fan_tach_signal_is_active());
    puts("PASS: relay IRQ uses no RTOS API; SysTick mirror, 100000 edges, wrap, timeout and recovery");
    return 0;
}
